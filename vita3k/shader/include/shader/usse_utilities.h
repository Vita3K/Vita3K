#pragma once

#include <SPIRV/SpvBuilder.h>
#include <shader/usse_program_analyzer.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

#include <gxm/types.h>

struct FeatureState;

namespace shader::usse::utils {

struct SpirvUtilFunctions {
    spv::Function *pack_f16{ nullptr };
    spv::Function *unpack_f16{ nullptr };
    spv::Function *pack_fx8{ nullptr };
    spv::Function *unpack_fx8{ nullptr };
};

spv::Id finalize(spv::Builder &b, spv::Id first, spv::Id second, const Swizzle4 swizz, const int offset, const Imm4 dest_mask);
spv::Id load(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand op, const Imm4 dest_mask, int shift_offset);
void store(spv::Builder &b, const SpirvShaderParameters &params, SpirvUtilFunctions &utils, const FeatureState &features, Operand dest, spv::Id source, std::uint8_t dest_mask, int off);

spv::Id unpack(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id target, const DataType type, Swizzle4 swizz, const Imm4 dest_mask,
    const int offset);

spv::Id unpack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id scalar, const DataType type);
spv::Id pack_one(spv::Builder &b, SpirvUtilFunctions &utils, const FeatureState &features, spv::Id vec, const DataType source_type);

spv::Id make_uniform_vector_from_type(spv::Builder &b, spv::Id type, int val);

template <typename F>
void make_for_loop(spv::Builder &b, spv::Id iterator, spv::Id initial_value_ite, spv::Id iterator_limit, F body) {
    auto blocks = b.makeNewLoop();
    b.createStore(initial_value_ite, iterator);
    b.createBranch(&blocks.head);

    b.setBuildPoint(&blocks.head);
    spv::Id compare_result = b.createOp(spv::OpSLessThan, b.makeBoolType(), { iterator, iterator_limit });

    b.createLoopMerge(&blocks.merge, &blocks.continue_target, spv::LoopControlMaskNone, {});
    b.createConditionalBranch(compare_result, &blocks.body, &blocks.merge);

    b.setBuildPoint(&blocks.body);
    body();

    // Increase i
    spv::Id add_to_me = b.createBinOp(spv::OpIAdd, b.makeIntegerType(32, true), b.createLoad(iterator), b.makeIntConstant(1));
    b.createStore(add_to_me, iterator);

    b.createBranch(&blocks.continue_target);

    b.setBuildPoint(&blocks.continue_target);
    b.createBranch(&blocks.head);

    b.setBuildPoint(&blocks.merge);
    b.closeLoop();
}
} // namespace shader::usse::utils