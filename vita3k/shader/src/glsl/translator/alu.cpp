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

#include <shader/decoder_helpers.h>
#include <shader/disasm.h>
#include <shader/glsl/code_writer.h>
#include <shader/glsl/params.h>
#include <shader/glsl/translator.h>
#include <shader/types.h>
#include <util/log.h>

using namespace shader;
using namespace usse;
using namespace glsl;

bool USSETranslatorVisitorGLSL::vmad(
    ExtVecPredicate pred,
    Imm1 skipinv,
    Imm1 gpi1_swiz_ext,
    Imm1 opcode2,
    Imm1 dest_use_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    RepeatMode repeat_mode,
    Imm1 gpi0_abs,
    RepeatCount repeat_count,
    bool nosched,
    Imm4 write_mask,
    Imm1 src1_neg,
    Imm1 src1_abs,
    Imm1 gpi1_neg,
    Imm1 gpi1_abs,
    Imm1 gpi0_swiz_ext,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 gpi0_n,
    Imm6 dest_n,
    Imm4 gpi0_swiz,
    Imm4 gpi1_swiz,
    Imm2 gpi1_n,
    Imm1 gpi0_neg,
    Imm1 src1_swiz_ext,
    Imm4 src1_swiz,
    Imm6 src1_n) {
    if (!USSETranslatorVisitor::vmad(pred, skipinv, gpi1_swiz_ext, opcode2, dest_use_bank_ext, end, src1_bank_ext, repeat_mode, gpi0_abs, repeat_count, nosched, write_mask, src1_neg, src1_abs,
            gpi1_neg, gpi1_abs, gpi0_swiz_ext, dest_bank, src1_bank, gpi0_n, dest_n, gpi0_swiz, gpi1_swiz, gpi1_n, gpi0_neg, src1_swiz_ext, src1_swiz, src1_n)) {
        return false;
    }

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, repeat_mode);

    std::string vsrc0 = variables.load(decoded_inst.opr.src0, write_mask, src0_repeat_offset);
    std::string vsrc1 = variables.load(decoded_inst.opr.src1, write_mask, src1_repeat_offset);
    std::string vsrc2 = variables.load(decoded_inst.opr.src2, write_mask, src2_repeat_offset);

    if (vsrc0.empty() || vsrc1.empty() || vsrc2.empty()) {
        LOG_ERROR("Source not loaded (vsrc0: {}, vsrc1: {}, vsrc2: {})", vsrc0, vsrc1, vsrc2);
        return false;
    }

    variables.store(decoded_inst.opr.dest, fmt::format("fma({}, {}, {})", vsrc0, vsrc1, vsrc2), write_mask, 0);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitorGLSL::vmad2(
    Imm1 dat_fmt,
    Imm2 pred,
    Imm1 skipinv,
    Imm1 src0_swiz_bits2,
    Imm1 syncstart,
    Imm1 src0_abs,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm3 src2_swiz,
    Imm1 src1_swiz_bit2,
    Imm1 nosched,
    Imm4 dest_mask,
    Imm2 src1_mod,
    Imm2 src2_mod,
    Imm1 src0_bank,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm6 dest_n,
    Imm2 src1_swiz_bits01,
    Imm2 src0_swiz_bits01,
    Imm6 src0_n,
    Imm6 src1_n,
    Imm6 src2_n) {
    if (!USSETranslatorVisitor::vmad2(dat_fmt, pred, skipinv, src0_swiz_bits2, syncstart, src0_abs, src1_bank_ext,
            src2_bank_ext, src2_swiz, src1_swiz_bit2, nosched, dest_mask, src1_mod, src2_mod, src0_bank, dest_bank,
            src1_bank, src2_bank, dest_n, src1_swiz_bits01, src0_swiz_bits01, src0_n, src1_n, src2_n)) {
        return false;
    }

    // Translate the instruction
    std::string vsrc0 = variables.load(decoded_inst.opr.src0, decoded_dest_mask, 0);
    std::string vsrc1 = variables.load(decoded_inst.opr.src1, decoded_dest_mask, 0);
    std::string vsrc2 = variables.load(decoded_inst.opr.src2, decoded_dest_mask, 0);

    if (vsrc0.empty() || vsrc1.empty() || vsrc2.empty()) {
        LOG_ERROR("Source not loaded (vsrc0: {}, vsrc1: {}, vsrc2: {})", vsrc0, vsrc1, vsrc2);
        return false;
    }

    variables.store(decoded_inst.opr.dest, fmt::format("fma({}, {}, {})", vsrc0, vsrc1, vsrc2), decoded_dest_mask, 0);
    return true;
}

bool USSETranslatorVisitorGLSL::vdp(
    ExtVecPredicate pred,
    Imm1 skipinv,
    bool clip_plane_enable,
    Imm1 opcode2,
    Imm1 dest_use_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    RepeatMode repeat_mode,
    Imm1 gpi0_abs,
    RepeatCount repeat_count,
    bool nosched,
    Imm4 write_mask,
    Imm1 src1_neg,
    Imm1 src1_abs,
    Imm3 clip_plane_n,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 gpi0_n,
    Imm6 dest_n,
    Imm4 gpi0_swiz,
    Imm3 src1_swiz_w,
    Imm3 src1_swiz_z,
    Imm3 src1_swiz_y,
    Imm3 src1_swiz_x,
    Imm6 src1_n) {
    if (!USSETranslatorVisitor::vdp(pred, skipinv, clip_plane_enable, opcode2, dest_use_bank_ext, end, src1_bank_ext,
            repeat_mode, gpi0_abs, repeat_count, nosched, write_mask, src1_neg, src1_abs, clip_plane_n, dest_bank,
            src1_bank, gpi0_n, dest_n, gpi0_swiz, src1_swiz_w, src1_swiz_z, src1_swiz_y, src1_swiz_x, src1_n)) {
        return false;
    }

    int type = 2;

    if (opcode2 == 0) {
        type = 1;
    }

    // Decoding done
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, repeat_mode)

    std::string lhs = variables.load(decoded_inst.opr.src1, type == 1 ? 0b0111 : 0b1111, src1_repeat_offset);
    std::string rhs = variables.load(decoded_inst.opr.src2, type == 1 ? 0b0111 : 0b1111, src2_repeat_offset);

    if (lhs.empty() || rhs.empty()) {
        LOG_ERROR("Source not loaded (lhs: {}, rhs: {})", lhs, rhs);
        return false;
    }

    std::string result = fmt::format("dot({}, {})", lhs, rhs);
    const std::size_t comp_count = dest_mask_to_comp_count(write_mask);

    if (comp_count > 1) {
        result = fmt::format("vec{}({})", comp_count, result);
    }

    variables.store(decoded_inst.opr.dest, result, write_mask, 0);

    // Rotate write mask
    write_mask = (write_mask << 1) | (write_mask >> 3);
    END_REPEAT()

    return true;
}

std::string USSETranslatorVisitorGLSL::do_alu_op(Instruction &inst, const Imm4 source_mask, const Imm4 possible_dest_mask) {
    std::string vsrc1 = variables.load(inst.opr.src1, source_mask, 0);
    std::string vsrc2 = variables.load(inst.opr.src2, source_mask, 0);

    if (vsrc1.empty() || vsrc2.empty()) {
        LOG_WARN("Could not find a src register");
        return "";
    }

    switch (inst.opcode) {
    case Opcode::AND: {
        return fmt::format("{} & {}", vsrc1, vsrc2);
        break;
    }

    case Opcode::OR: {
        if (vsrc1 == "0") {
            return vsrc2;
        } else if (vsrc2 == "0") {
            return vsrc1;
        }

        return fmt::format("{} | {}", vsrc1, vsrc2);
    }

    case Opcode::XOR: {
        return fmt::format("{} ^ {}", vsrc1, vsrc2);
    }

    case Opcode::SHL: {
        return fmt::format("{} << {}", vsrc1, vsrc2);
    }

    // TODO: May need to separate them!
    case Opcode::SHR:
    case Opcode::ASR: {
        return fmt::format("{} >> {}", vsrc1, vsrc2);
    }

    case Opcode::VDSX:
    case Opcode::VF16DSX:
        return fmt::format("dFdx({})", vsrc1);

    case Opcode::VDSY:
    case Opcode::VF16DSY:
        return fmt::format("dFdy({})", vsrc1);

    case Opcode::VADD:
    case Opcode::VF16ADD: {
        return fmt::format("{} + {}", vsrc1, vsrc2);
    }

    case Opcode::VMUL:
    case Opcode::VF16MUL: {
        return fmt::format("{} * {}", vsrc1, vsrc2);
    }

    case Opcode::VMIN:
    case Opcode::VF16MIN: {
        return fmt::format("min({}, {})", vsrc1, vsrc2);
    }

    case Opcode::VMAX:
    case Opcode::VF16MAX: {
        return fmt::format("max({}, {})", vsrc1, vsrc2);
    }

    case Opcode::VFRC:
    case Opcode::VF16FRC: {
        // Dest = Source1 - Floor(Source2)
        // If two source are identical, let's use the fractional function
        if (inst.opr.src1.is_same(inst.opr.src2, source_mask)) {
            return fmt::format("fract({})", vsrc1);
        }

        return fmt::format("{} - floor({})", vsrc1, vsrc2);
    }

    case Opcode::VDP:
    case Opcode::VF16DP: {
        std::string res = fmt::format("dot({}, {})", vsrc1, vsrc2);
        std::size_t total_dest = dest_mask_to_comp_count(possible_dest_mask);
        if (total_dest > 1) {
            return fmt::format("vec{}({})", total_dest, res);
        }
        return res;
    }

    case Opcode::FPSUB8:
    case Opcode::ISUBU16:
    case Opcode::ISUB16:
    case Opcode::ISUBU32:
    case Opcode::ISUB32:
        return fmt::format("{} - {}", vsrc1, vsrc2);

    default: {
        LOG_ERROR("Unimplemented ALU opcode instruction: {}", disasm::opcode_str(inst.opcode));
        break;
    }
    }

    return "";
}

bool USSETranslatorVisitorGLSL::v32nmad(
    ExtVecPredicate pred,
    bool skipinv,
    Imm2 src1_swiz_10_11,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 src1_swiz_9,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm4 src2_swiz,
    bool nosched,
    Imm4 dest_mask,
    Imm2 src1_mod,
    Imm1 src2_mod,
    Imm2 src1_swiz_7_8,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm2 src2_bank_sel,
    Imm6 dest_n,
    Imm7 src1_swiz_0_6,
    Imm3 op2,
    Imm6 src1_n,
    Imm6 src2_n) {
    if (!USSETranslatorVisitor::v32nmad(pred, skipinv, src1_swiz_10_11, syncstart, dest_bank_ext, src1_swiz_9,
            src1_bank_ext, src2_bank_ext, src2_swiz, nosched, dest_mask, src1_mod, src2_mod, src1_swiz_7_8,
            dest_bank_sel, src1_bank_sel, src2_bank_sel, dest_n, src1_swiz_0_6, op2, src1_n, src2_n)) {
        return false;
    }

    // Recompile
    std::string result = do_alu_op(decoded_inst, decoded_source_mask, dest_mask);

    if (!result.empty()) {
        variables.store(decoded_inst.opr.dest, result, dest_mask, 0);
    }

    return true;
}

bool USSETranslatorVisitorGLSL::vcomp(
    ExtPredicate pred,
    bool skipinv,
    Imm2 dest_type,
    bool syncstart,
    bool dest_bank_ext,
    bool end,
    bool src1_bank_ext,
    RepeatCount repeat_count,
    bool nosched,
    Imm2 op2,
    Imm2 src_type,
    Imm2 src1_mod,
    Imm2 src_comp,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm7 dest_n,
    Imm7 src1_n,
    Imm4 write_mask) {
    if (!USSETranslatorVisitor::vcomp(pred, skipinv, dest_type, syncstart, dest_bank_ext, end, src1_bank_ext,
            repeat_count, nosched, op2, src_type, src1_mod, src_comp, dest_bank, src1_bank, dest_n, src1_n,
            write_mask)) {
        return false;
    }

    // TODO: Log
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    std::string result = variables.load(decoded_inst.opr.src1, decoded_source_mask, src1_repeat_offset);

    if (result.empty()) {
        LOG_ERROR("Result not loaded");
        return false;
    }

    std::size_t num_comp = dest_mask_to_comp_count(decoded_source_mask);

    switch (decoded_inst.opcode) {
    case Opcode::VRCP: {
        std::size_t dest_num_comp = dest_mask_to_comp_count(write_mask);

        if (num_comp != dest_num_comp && num_comp == 1) {
            result = fmt::format("vec{}({})", dest_num_comp, result);
        } else if (num_comp == 1) {
            result = std::string("1.0 / ") + result;
        } else {
            result = fmt::format("vec{}(1.0) / {}", num_comp, result);
        }
        break;
    }

    case Opcode::VRSQ: {
        result = fmt::format("inversesqrt({})", result);
        break;
    }

    case Opcode::VLOG: {
        result = fmt::format("log({})", result);
        break;
    }

    case Opcode::VEXP: {
        result = fmt::format("exp({})", result);
        break;
    }

    default:
        break;
    }

    variables.store(decoded_inst.opr.dest, result, write_mask, dest_repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitorGLSL::sop2(
    Imm2 pred,
    Imm1 cmod1,
    Imm1 skipinv,
    Imm1 nosched,
    Imm2 asel1,
    Imm1 dest_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm1 cmod2,
    Imm3 count,
    Imm1 amod1,
    Imm2 asel2,
    Imm3 csel1,
    Imm3 csel2,
    Imm1 amod2,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm1 src1_mod,
    Imm2 cop,
    Imm2 aop,
    Imm1 asrc1_mod,
    Imm1 dest_mod,
    Imm7 src1_n,
    Imm7 src2_n) {
    static auto selector_zero = [](const std::string src1_color, const std::string src2_color, const std::string src1_alpha, const std::string src2_alpha, const bool is_color) {
        return is_color ? "uvec3(0)" : "0";
    };

    static auto selector_src1_color = [](const std::string src1_color, const std::string src2_color, const std::string src1_alpha, const std::string src2_alpha, const bool is_color) {
        return src1_color;
    };

    static auto selector_src2_color = [](const std::string src1_color, const std::string src2_color, const std::string src1_alpha, const std::string src2_alpha, const bool is_color) {
        return src2_color;
    };

    static auto selector_src1_alpha = [](const std::string src1_color, const std::string src2_color, const std::string src1_alpha, const std::string src2_alpha, const bool is_color) {
        if (is_color) {
            return fmt::format("uvec3({})", src1_alpha);
        }

        return src1_alpha;
    };

    static auto selector_src2_alpha = [](const std::string src1_color, const std::string src2_color, const std::string src1_alpha, const std::string src2_alpha, const bool is_color) {
        if (is_color) {
            return fmt::format("uvec3({})", src2_alpha);
        }

        return src2_alpha;
    };

    // This opcode always operates on C10.
    static Opcode operations[] = {
        Opcode::FADD,
        Opcode::FSUB,
        Opcode::FMIN,
        Opcode::FMAX
    };

    using SelectorFunc = std::function<std::string(const std::string, const std::string, const std::string, const std::string, bool)>;

    static SelectorFunc color_selector_1[] = {
        selector_zero,
        selector_src1_color,
        selector_src2_color,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr, // source alpha saturated.
        nullptr // source alpha scale
    };

    static SelectorFunc color_selector_2[] = {
        selector_zero,
        selector_src1_color,
        selector_src2_color,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr, // source alpha saturated.
        nullptr // zero source 2 minus half.
    };

    static SelectorFunc alpha_selector_1[] = {
        selector_zero,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr // source 2 scale.
    };

    static SelectorFunc alpha_selector_2[] = {
        selector_zero,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr // zero source 2 minus half.
    };

    Instruction inst;
    inst.opcode = Opcode::SOP2;

    // Never double register num
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_bank_ext, false, 7, m_second_program);

    inst.opr.src1.type = DataType::UINT8;
    inst.opr.src2.type = DataType::UINT8;
    inst.opr.dest.type = DataType::UINT8;

    if (cop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid color opcode: {}", (int)cop);
        return true;
    }

    if (aop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid alpha opcode: {}", (int)aop);
        return true;
    }

    Opcode color_op = operations[cop];
    Opcode alpha_op = operations[aop];

    if (csel1 >= sizeof(color_selector_1) / sizeof(SelectorFunc) || csel2 >= sizeof(color_selector_2) / sizeof(SelectorFunc) || asel1 >= sizeof(alpha_selector_1) / sizeof(SelectorFunc) || asel2 >= sizeof(alpha_selector_2) / sizeof(SelectorFunc)) {
        LOG_ERROR("Unknown color/alpha selector (csel1: {}, csel2: {}, asel1: {}, asel2: {}", csel1, csel2,
            asel1, asel2);
        return true;
    }

    // Lookup color selector
    SelectorFunc color_selector_1_func = color_selector_1[csel1];
    SelectorFunc color_selector_2_func = color_selector_2[csel2];
    SelectorFunc alpha_selector_1_func = alpha_selector_1[asel1];
    SelectorFunc alpha_selector_2_func = alpha_selector_2[asel2];

    if (!color_selector_1_func || !color_selector_2_func || !alpha_selector_1_func || !alpha_selector_2_func) {
        LOG_ERROR("Unimplemented color/alpha selector (csel1: {}, csel2: {}, asel1: {}, asel2: {}", csel1, csel2,
            asel1, asel2);
        return true;
    }

    auto apply_opcode = [&](Opcode op, std::string lhs, std::string rhs) -> std::string {
        switch (op) {
        case Opcode::FADD: {
            return fmt::format("{} + {}", lhs, rhs);
        }

        case Opcode::FSUB: {
            return fmt::format("{} - {}", lhs, rhs);
        }

        case Opcode::FMIN:
        case Opcode::VMIN: {
            return fmt::format("min({}, {})", lhs, rhs);
        }

        case Opcode::FMAX:
        case Opcode::VMAX: {
            return fmt::format("max({}, {})", lhs, rhs);
        }

        default: {
            LOG_ERROR("Unsuppported sop opcode: {}", disasm::opcode_str(op));
            break;
        }
        }

        return "";
    };

    auto apply_complement_modifiers = [&](std::string &target, Imm1 enable, const bool is_rgb) {
        if (enable == 1) {
            if (is_rgb) {
                target = fmt::format("(uvec3(255) - {})", target);
            } else {
                target = fmt::format("(255 - {})", target);
            }
        }
    };

    BEGIN_REPEAT(count)
    GET_REPEAT(inst, RepeatMode::SLMSI)

    std::string src1_color = variables.load(inst.opr.src1, 0b0111, src1_repeat_offset);
    std::string src2_color = variables.load(inst.opr.src2, 0b0111, src2_repeat_offset);
    std::string src1_alpha = variables.load(inst.opr.src1, 0b1000, src1_repeat_offset);
    std::string src2_alpha = variables.load(inst.opr.src2, 0b1000, src2_repeat_offset);

    // Apply source flag.
    // Complement is the vector that when added with source creates a nice vec4(1.0)
    // In the case of GLSL implementation here, to avoid casting to float, we use 255 instead
    apply_complement_modifiers(src1_color, src1_mod, true);
    apply_complement_modifiers(src1_alpha, asrc1_mod, false);

    // Apply color selector
    std::string factored_rgb_lhs = color_selector_1_func(src1_color, src2_color, src1_alpha, src2_alpha, true);
    std::string factored_rgb_rhs = color_selector_2_func(src1_color, src2_color, src1_alpha, src2_alpha, true);

    std::string factored_a_lhs = alpha_selector_1_func(src1_color, src2_color, src1_alpha, src2_alpha, false);
    std::string factored_a_rhs = alpha_selector_2_func(src1_color, src2_color, src1_alpha, src2_alpha, false);

    // Apply modifiers
    apply_complement_modifiers(factored_rgb_lhs, cmod1, true);
    apply_complement_modifiers(factored_rgb_rhs, cmod2, true);
    apply_complement_modifiers(factored_a_lhs, amod1, false);
    apply_complement_modifiers(factored_a_rhs, amod2, false);

    // Factor them with source
    factored_rgb_lhs = fmt::format("{} * {}", factored_rgb_lhs, src1_color);
    factored_rgb_rhs = fmt::format("{} * {}", factored_rgb_rhs, src2_color);

    factored_a_lhs = fmt::format("{} * {}", factored_a_lhs, src1_alpha);
    factored_a_rhs = fmt::format("{} * {}", factored_a_rhs, src2_alpha);

    auto color_res = apply_opcode(color_op, factored_rgb_lhs, factored_rgb_rhs);
    auto alpha_res = apply_opcode(alpha_op, factored_a_lhs, factored_a_rhs);

    color_res = fmt::format("({}) / 255", color_res);
    alpha_res = fmt::format("({}) / 255", alpha_res);

    // Final result. Do binary operation and then store
    variables.store(inst.opr.dest, color_res, 0b0111, dest_repeat_offset);
    variables.store(inst.opr.dest, alpha_res, 0b1000, dest_repeat_offset);

    // TODO log correctly
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(color_op));
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(alpha_op));

    END_REPEAT()

    return true;
}

bool USSETranslatorVisitorGLSL::sop2m(Imm2 pred,
    Imm1 mod1,
    Imm1 skipinv,
    Imm1 nosched,
    Imm2 cop,
    Imm1 destbankext,
    Imm1 end,
    Imm1 src1bankext,
    Imm1 src2bankext,
    Imm1 mod2,
    Imm4 wmask,
    Imm2 aop,
    Imm3 sel1,
    Imm3 sel2,
    Imm2 destbank,
    Imm2 src1bank,
    Imm2 src2bank,
    Imm7 destnum,
    Imm7 src1num,
    Imm7 src2num) {
    static auto selector_zero = [](const std::string src1, const std::string src2) -> std::string {
        return "uvec4(0)";
    };

    static auto selector_src1_color = [](const std::string src1, const std::string src2) {
        return src1;
    };

    static auto selector_src2_color = [](const std::string src1, const std::string src2) {
        return src2;
    };

    static auto selector_src1_alpha = [](const std::string src1, const std::string src2) {
        return fmt::format("{}.wwww", src1);
    };

    static auto selector_src2_alpha = [](const std::string src1, const std::string src2) {
        return fmt::format("{}.wwww", src2);
    };

    // This opcode always operates on C10.
    static Opcode operations[] = {
        Opcode::FADD,
        Opcode::FSUB,
        Opcode::FMIN,
        Opcode::FMAX
    };

    using SelectorFunc = std::function<std::string(const std::string, const std::string)>;

    static SelectorFunc selector[] = {
        selector_zero,
        nullptr, // source alpha saturated.
        selector_src1_color,
        selector_src1_alpha,
        nullptr,
        nullptr,
        selector_src2_color,
        selector_src2_alpha
    };

    Instruction inst;
    inst.opcode = Opcode::SOP2WM;

    // Never double register num
    inst.opr.src1 = decode_src12(inst.opr.src1, src1num, src1bank, src1bankext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2num, src2bank, src2bankext, false, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, destnum, destbank, destbankext, false, 7, m_second_program);

    inst.opr.src1.type = DataType::UINT8;
    inst.opr.src2.type = DataType::UINT8;
    inst.opr.dest.type = DataType::UINT8;

    if (cop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid color opcode: {}", (int)cop);
        return true;
    }

    if (aop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid alpha opcode: {}", (int)aop);
        return true;
    }

    Opcode color_op = operations[cop];
    Opcode alpha_op = operations[aop];

    // Lookup color selector
    SelectorFunc operation_1_lhs_selector_func = selector[sel1];
    SelectorFunc operation_2_lhs_selector_func = selector[sel2];

    if (!operation_1_lhs_selector_func || !operation_2_lhs_selector_func) {
        LOG_ERROR("Unimplemented operation selector (sel1: {}, sel2: {}", sel1, sel2);
        return true;
    }

    auto apply_opcode = [&](Opcode op, std::string lhs, std::string rhs) -> std::string {
        switch (op) {
        case Opcode::FADD: {
            return fmt::format("{} + {}", lhs, rhs);
        }

        case Opcode::FSUB: {
            return fmt::format("{} - {}", lhs, rhs);
        }

        case Opcode::FMIN:
        case Opcode::VMIN: {
            return fmt::format("min({}, {})", lhs, rhs);
        }

        case Opcode::FMAX:
        case Opcode::VMAX: {
            return fmt::format("max({}, {})", lhs, rhs);
        }

        default: {
            LOG_ERROR("Unsuppported sop opcode: {}", disasm::opcode_str(op));
            break;
        }
        }

        return "";
    };

    auto apply_complement_modifiers = [&](std::string &target, Imm1 enable) {
        if (enable == 1) {
            target = fmt::format("uvec4(255) - {}", target);
        }
    };

    std::string src1 = variables.load(inst.opr.src1, 0b1111, 0);
    std::string src2 = variables.load(inst.opr.src2, 0b1111, 0);

    // Apply selector
    std::string operation_1 = operation_1_lhs_selector_func(src1, src2);
    std::string operation_2 = operation_2_lhs_selector_func(src1, src2);

    // Apply modifiers
    apply_complement_modifiers(operation_1, mod1);
    apply_complement_modifiers(operation_2, mod2);

    // Factor them with source
    operation_1 = fmt::format("{} * {}", operation_1, src1);
    operation_2 = fmt::format("{} * {}", operation_2, src2);

    std::string result = apply_opcode(color_op, operation_1, operation_2);

    // Alpha is at the first bit, so reverse that
    wmask = ((wmask & 0b1110) >> 1) | ((wmask & 1) << 3);

    if (wmask & 0b111) {
        std::string swizz;
        for (std::uint8_t i = 0; i < 3; i++) {
            if (wmask & (1 << i)) {
                swizz += static_cast<char>('w' + (i + 1) % 4);
            }
        }
        result = fmt::format("({}).{} / 255", result, swizz);
        variables.store(inst.opr.dest, result, wmask & 0b111, 0);
    }

    if (wmask & 0b1000) {
        // Alpha is written, so calculate and also store
        variables.store(inst.opr.dest, fmt::format("({}).w / 255", apply_opcode(alpha_op, operation_1, operation_2)), 0b1000, 0);
    }

    // TODO log correctly
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(color_op));
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(alpha_op));

    return true;
}

bool USSETranslatorVisitorGLSL::sop3(Imm2 pred,
    Imm1 mod1,
    Imm1 skipinv,
    Imm1 nosched,
    Imm2 cop,
    Imm1 destbext,
    Imm1 end,
    Imm1 src1bext,
    Imm1 src2bext,
    Imm1 mod2,
    Imm4 wrmask,
    Imm2 aop,
    Imm3 sel1,
    Imm3 sel2,
    Imm1 src0bank,
    Imm2 destbank,
    Imm2 src1bank,
    Imm2 src2bank,
    Imm7 destn,
    Imm7 src0n,
    Imm7 src1n,
    Imm7 src2n) {
    LOG_DISASM("Unimplmenet Opcode: sop3");
    return true;
}

static int calculate_num_source_need_for_dual_inst(Opcode op) {
    switch (op) {
    case Opcode::VDP:
    case Opcode::VADD:
    case Opcode::FADD:
    case Opcode::VMUL:
    case Opcode::FMUL:
        return 2;

    case Opcode::FMAD:
    case Opcode::VMAD:
        return 3;
    }

    return 1;
}

bool USSETranslatorVisitorGLSL::vdual(
    Imm1 comp_count_type,
    Imm1 gpi1_neg,
    Imm2 sv_pred,
    Imm1 skipinv,
    Imm1 dual_op1_ext_vec3_or_has_w_vec4,
    bool type_f16,
    Imm1 gpi1_swizz_ext,
    Imm4 unified_store_swizz,
    Imm1 unified_store_neg,
    Imm3 dual_op1,
    Imm1 dual_op2_ext,
    bool prim_ustore,
    Imm4 gpi0_swizz,
    Imm4 gpi1_swizz,
    Imm2 prim_dest_bank,
    Imm2 unified_store_slot_bank,
    Imm2 prim_dest_num_gpi_case,
    Imm7 prim_dest_num,
    Imm3 dual_op2,
    Imm2 src_config,
    Imm1 gpi2_slot_num_bit_1,
    Imm1 gpi2_slot_num_bit_0_or_unified_store_abs,
    Imm2 gpi1_slot_num,
    Imm2 gpi0_slot_num,
    Imm3 write_mask_non_gpi,
    Imm7 unified_store_slot_num) {
    if (!USSETranslatorVisitor::vdual(comp_count_type, gpi1_neg, sv_pred, skipinv, dual_op1_ext_vec3_or_has_w_vec4,
            type_f16, gpi1_swizz_ext, unified_store_swizz, unified_store_neg, dual_op1, dual_op2_ext, prim_ustore,
            gpi0_swizz, gpi1_swizz, prim_dest_bank, unified_store_slot_bank, prim_dest_num_gpi_case,
            prim_dest_num, dual_op2, src_config, gpi2_slot_num_bit_1, gpi2_slot_num_bit_0_or_unified_store_abs,
            gpi1_slot_num, gpi0_slot_num, write_mask_non_gpi, unified_store_slot_num)) {
        return false;
    }

    std::string src0_op2, src1_op2, src2_op2;

    int num_comp = dest_mask_to_comp_count(decoded_source_mask_2);
    std::string temp_load = variables.load(decoded_inst_2.opr.src0, decoded_source_mask_2, 0);

    src0_op2 = fmt::format("temp{}", num_comp);
    writer.add_to_current_body(fmt::format("{} = {};", src0_op2, temp_load));

    int num_src_needed = calculate_num_source_need_for_dual_inst(decoded_inst_2.opcode);
    if (num_src_needed > 1) {
        temp_load = variables.load(decoded_inst_2.opr.src1, decoded_source_mask_2, 0);

        src1_op2 = fmt::format("temp{}_1", num_comp);
        writer.add_to_current_body(fmt::format("{} = {};", src1_op2, temp_load));
    }

    if (num_src_needed > 2) {
        temp_load = variables.load(decoded_inst_2.opr.src2, decoded_source_mask_2, 0);

        src2_op2 = fmt::format("temp{}_2", num_comp);
        writer.add_to_current_body(fmt::format("{} = {};", src2_op2, temp_load));
    }

    std::string src0_op1, src1_op1, src2_op1;

    num_src_needed = calculate_num_source_need_for_dual_inst(decoded_inst.opcode);
    src0_op1 = variables.load(decoded_inst.opr.src0, decoded_source_mask, 0);
    if (num_src_needed > 1) {
        src1_op1 = variables.load(decoded_inst.opr.src1, decoded_source_mask, 0);
    }
    if (num_src_needed > 2) {
        src2_op1 = variables.load(decoded_inst.opr.src2, decoded_source_mask, 0);
    }

    auto do_dual_op = [&](Opcode op, const std::string &src0, const std::string &src1, const std::string &src2, const Imm4 write_mask_source) -> std::string {
        switch (op) {
        case Opcode::VMOV: {
            return src0;
        }
        case Opcode::FADD:
        case Opcode::VADD: {
            return fmt::format("{} + {}", src0, src1);
        }
        case Opcode::FRCP: {
            if (dest_mask_to_comp_count(write_mask_source) > 1) {
                return fmt::format("vec{}(1.0) / {}", dest_mask_to_comp_count(write_mask_source), src0);
            }

            return fmt::format("1.0 / {}", src0);
        }

        case Opcode::FRSQ: {
            return fmt::format("inversesqrt({})", src0);
        }

        case Opcode::FMUL:
        case Opcode::VMUL: {
            return fmt::format("{} * {}", src0, src1);
        }
        case Opcode::VDP: {
            return fmt::format("dot({}, {})", src0, src1);
        }
        case Opcode::FEXP: {
            return fmt::format("exp({})", src0);
        }
        case Opcode::FLOG: {
            return fmt::format("log({})", src0);
        }
        case Opcode::VSSQ: {
            return fmt::format("dot({}, {})", src0, src0);
        }
        case Opcode::FMAD:
        case Opcode::VMAD: {
            return fmt::format("fma({}, {}, {})", src0, src1, src2);
        }
        default:
            LOG_ERROR("Missing implementation for DUAL {}.", disasm::opcode_str(op));
            break;
        }

        return "";
    };

    std::string op1_result = do_dual_op(decoded_inst.opcode, src0_op1, src1_op1, src2_op1, decoded_source_mask);
    if (op1_result.empty())
        return false;
    std::string op2_result = do_dual_op(decoded_inst_2.opcode, src0_op2, src1_op2, src2_op2, decoded_source_mask_2);
    if (op2_result.empty())
        return false;

    if ((dest_mask_to_comp_count(decoded_source_mask) == 1) || ((decoded_inst.opcode == Opcode::VSSQ) || (decoded_inst.opcode == Opcode::VDP))) {
        if (dest_mask_to_comp_count(decoded_dest_mask) > 1) {
            op1_result = fmt::format("vec{}({})", dest_mask_to_comp_count(decoded_dest_mask), op1_result);
        }
    }

    if ((dest_mask_to_comp_count(decoded_source_mask_2) == 1) || ((decoded_inst_2.opcode == Opcode::VSSQ) || (decoded_inst_2.opcode == Opcode::VDP))) {
        if (dest_mask_to_comp_count(decoded_dest_mask_2) > 1) {
            op2_result = fmt::format("vec{}({})", dest_mask_to_comp_count(decoded_dest_mask_2), op2_result);
        }
    }

    // Dual instructions are async. To simulate that we store after both instructions are completed.
    variables.store(decoded_inst.opr.dest, op1_result, decoded_dest_mask, 0);
    variables.store(decoded_inst_2.opr.dest, op2_result, decoded_dest_mask_2, 0);

    return true;
}