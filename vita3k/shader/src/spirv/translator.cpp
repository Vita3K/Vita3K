// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <shader/spirv/translator.h>

#include <fmt/format.h>
#include <gxm/types.h>
#include <shader/disasm.h>
#include <shader/translator_types.h>
#include <util/log.h>
#include <util/optional.h>

#include <map>

namespace shader::usse {
//
// Decoder/translator usage
//

USSERecompilerSpirv::USSERecompilerSpirv(spv::Builder &b, const SceGxmProgram &program, const FeatureState &features, const SpirvShaderParameters &parameters,
    utils::SpirvUtilFunctions &utils, const NonDependentTextureQueryCallInfos &queries, const spv::Id render_info_id)
    : USSERecompiler(program)
    , b(b) {
    visitor = std::make_unique<USSETranslatorVisitorSpirv>(b, *this, program, features, utils, cur_instr, parameters, queries, render_info_id, false);
}

USSETranslatorVisitorSpirv *USSERecompilerSpirv::get_spirv_translator_visitor() {
    return reinterpret_cast<USSETranslatorVisitorSpirv *>(visitor.get());
}

spv::Id USSERecompilerSpirv::get_condition_value(const std::uint8_t pred, const bool neg) {
    const ExtPredicate predicator = static_cast<ExtPredicate>(pred);

    Operand pred_opr{};
    pred_opr.bank = RegisterBank::PREDICATE;

    bool do_neg = neg;

    if (predicator >= ExtPredicate::P0 && predicator <= ExtPredicate::P3) {
        pred_opr.num = static_cast<int>(predicator) - static_cast<int>(ExtPredicate::P0);
    } else if (predicator >= ExtPredicate::NEGP0 && predicator <= ExtPredicate::NEGP1) {
        pred_opr.num = static_cast<int>(predicator) - static_cast<int>(ExtPredicate::NEGP0);
        do_neg = !do_neg;
    }

    spv::Id pred_v = get_spirv_translator_visitor()->load(pred_opr, 0b0001);
    if (do_neg) {
        std::vector<spv::Id> ops{ pred_v };
        pred_v = b.createOp(spv::OpLogicalNot, b.makeBoolType(), ops);
    }

    return pred_v;
}

void USSERecompilerSpirv::compile_break_node(const usse::USSEBreakNode &node) {
    std::unique_ptr<spv::Builder::If> cond_builder;

    if (node.get_condition() != 0) {
        spv::Id pred_v = get_condition_value(node.get_condition());
        cond_builder = std::make_unique<spv::Builder::If>(pred_v, spv::SelectionControlMaskNone, b);
    }

    b.createLoopExit();

    if (cond_builder)
        cond_builder->makeEndIf();
}

void USSERecompilerSpirv::compile_continue_node(const usse::USSEContinueNode &node) {
    std::unique_ptr<spv::Builder::If> cond_builder;

    if (node.get_condition() != 0) {
        spv::Id pred_v = get_condition_value(node.get_condition());
        cond_builder = std::make_unique<spv::Builder::If>(pred_v, spv::SelectionControlMaskNone, b);
    }

    b.createLoopContinue();

    if (cond_builder)
        cond_builder->makeEndIf();
}

void USSERecompilerSpirv::compile_conditional_node(const usse::USSEConditionalNode &cond) {
    spv::Builder::If if_builder(get_condition_value(cond.negif_condition(), true), spv::SelectionControlMaskNone, b);
    compile_block(*cond.if_block());

    if (cond.else_block()) {
        if_builder.makeBeginElse();
        compile_block(*cond.else_block());
    }

    if_builder.makeEndIf();
}

void USSERecompilerSpirv::compile_loop_node(const usse::USSELoopNode &loop) {
    spv::Builder::LoopBlocks loops = b.makeNewLoop();

    b.createBranch(&loops.head);
    b.setBuildPoint(&loops.head);

    // In the head we only want to branch to body. We always do while do anyway
    b.createLoopMerge(&loops.merge, &loops.head, 0, {});
    b.createBranch(&loops.body);

    // Emit body content
    b.setBuildPoint(&loops.body);

    // If true
    const usse::USSEBlockNode &content_block = *(loop.content_block());
    compile_block(content_block);

    b.createBranch(&loops.continue_target);

    // Emit continue target
    b.setBuildPoint(&loops.continue_target);
    b.createBranch(&loops.head);

    // Merge point
    b.setBuildPoint(&loops.merge);
    b.closeLoop();
}

void USSERecompilerSpirv::begin_condition(const int cond) {
    cond_stacks.emplace(get_condition_value(cond), spv::SelectionControlMaskNone, b);
}

void USSERecompilerSpirv::end_condition() {
    cond_stacks.top().makeEndIf();
    cond_stacks.pop();
}

spv::Function *USSERecompilerSpirv::compile_program_function() {
    // Make a new function (subroutine)
    spv::Block *last_build_point = b.getBuildPoint();
    spv::Block *new_sub_block = nullptr;

    const auto sub_name = fmt::format("{}_program", visitor->is_translating_secondary_program() ? "secondary" : "primary");

    spv::Function *ret_func = b.makeFunctionEntry(spv::NoPrecision, b.makeVoidType(), sub_name.c_str(), {}, {},
        &new_sub_block);

    compile_block(tree_block_node);

    b.leaveFunction();
    b.setBuildPoint(last_build_point);

    return ret_func;
}

void convert_gxp_usse_to_spirv(spv::Builder &b, const SceGxmProgram &program, const FeatureState &features, const SpirvShaderParameters &parameters, utils::SpirvUtilFunctions &utils,
    spv::Function *begin_hook_func, spv::Function *end_hook_func, const NonDependentTextureQueryCallInfos &queries, const spv::Id render_info_id) {
    const uint64_t *primary_program = program.primary_program_start();
    const uint64_t primary_program_instr_count = program.primary_program_instr_count;

    const uint64_t *secondary_program_start = program.secondary_program_start();
    const uint64_t *secondary_program_end = program.secondary_program_end();

    std::map<ShaderPhase, std::pair<const std::uint64_t *, std::uint64_t>> shader_code;

    // Collect instructions of Pixel (primary) phase
    shader_code[ShaderPhase::Pixel] = std::make_pair(primary_program, primary_program_instr_count);

    // Collect instructions of Sample rate (secondary) phase
    shader_code[ShaderPhase::SampleRate] = std::make_pair(secondary_program_start, secondary_program_end - secondary_program_start);

    if (begin_hook_func)
        b.createFunctionCall(begin_hook_func, {});

    // Decode and recompile
    // TODO: Reuse this
    usse::USSERecompilerSpirv recomp(b, program, features, parameters, utils, queries, render_info_id);

    // Set the program
    recomp.program = &program;

    for (auto phase = 0; phase < (uint32_t)ShaderPhase::Max; ++phase) {
        const auto cur_phase_code = shader_code[(ShaderPhase)phase];

        if (cur_phase_code.second != 0) {
            if (static_cast<ShaderPhase>(phase) == ShaderPhase::SampleRate) {
                recomp.visitor->set_secondary_program(true);
            } else {
                recomp.visitor->set_secondary_program(false);
            }

            recomp.reset(cur_phase_code.first, cur_phase_code.second);
            b.createFunctionCall(recomp.compile_program_function(), {});
        }
    }

    // We reach the end
    // Call end hook. If it's discard, this is not even called, so no worry
    b.createFunctionCall(end_hook_func, {});

    std::vector<spv::Id> empty_args;
    if (features.should_use_shader_interlock() && program.is_fragment() && program.is_frag_color_used())
        b.createNoResultOp(spv::OpEndInvocationInterlockEXT);
}

} // namespace shader::usse
