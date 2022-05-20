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

#include <shader/glsl/params.h>
#include <shader/glsl/translator.h>
#include <shader/types.h>
#include <util/log.h>

using namespace shader;
using namespace usse;
using namespace glsl;

bool USSETranslatorVisitorGLSL::vbw(
    Imm3 op1,
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 nosched,
    bool repeat_sel,
    Imm1 sync_start,
    Imm1 dest_ext,
    Imm1 end,
    Imm1 src1_ext,
    Imm1 src2_ext,
    RepeatCount repeat_count,
    Imm1 src2_invert,
    Imm5 src2_rot,
    Imm2 src2_exth,
    Imm1 op2,
    Imm1 bitwise_partial,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src2_sel,
    Imm7 src1_n,
    Imm7 src2_n) {
    if (!USSETranslatorVisitor::vbw(op1, pred, skipinv, nosched, repeat_sel, sync_start, dest_ext, end, src1_ext, src2_ext,
            repeat_count, src2_invert, src2_rot, src2_exth, op2, bitwise_partial, dest_bank, src1_bank, src2_bank,
            dest_n, src2_sel, src1_n, src2_n)) {
        return false;
    }

    set_repeat_multiplier(1, 1, 1, 1);

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    bool immediate = src2_ext && decoded_inst.opr.src2.bank == RegisterBank::IMMEDIATE;
    uint32_t value = 0;

    if (src2_rot) {
        LOG_WARN("Bitwise Rotations are unsupported.");
        return false;
    }

    std::uint32_t src2_constant = 0;

    if (immediate) {
        src2_constant = src2_n | (static_cast<uint32_t>(src2_sel) << 7) | (static_cast<uint32_t>(src2_exth) << 14);
        if (src2_invert) {
            src2_constant = ~src2_constant;
        }
    }

    std::string operator_str;
    switch (decoded_inst.opcode) {
    case Opcode::OR: operator_str = "|"; break;
    case Opcode::AND: operator_str = "&"; break;
    case Opcode::XOR: operator_str = "^"; break;
    case Opcode::ROL:
        LOG_WARN("Bitwise Rotate Left operation unsupported.");
        return false; // TODO: SPIRV doesn't seem to have a rotate left operation!
    case Opcode::ASR: operator_str = ">>"; break;
    case Opcode::SHL: operator_str = "<<"; break;
    case Opcode::SHR: operator_str = ">>"; break;
    default: return false;
    }
    std::string calc_result;
    // optimisation. (any OR 0 || any XOR 0 || any AND 0xFFFFFFFF) -> assign
    if ((decoded_inst.opcode == Opcode::OR || decoded_inst.opcode == Opcode::XOR) && immediate && src2_constant == 0
        || ((decoded_inst.opcode == Opcode::AND) && immediate && src2_constant == std::numeric_limits<decltype(src2_constant)>::max())) {
        if (decoded_inst.opr.dest.is_same(decoded_inst.opr.src1, 0b0001)) {
            // Do nothing actually
            return true;
        }

        decoded_inst.opr.src1.type = DataType::F32;
        decoded_inst.opr.dest.type = DataType::F32;
        calc_result = variables.load(decoded_inst.opr.src1, 0b0001, src1_repeat_offset);
    } else {
        std::string src1 = variables.load(decoded_inst.opr.src1, 0b0001, src1_repeat_offset);
        std::string src2 = immediate ? fmt::format("{}", src2_constant) : variables.load(decoded_inst.opr.src2, 0b0001, src2_repeat_offset);
        if (src1.empty() || src2.empty()) {
            LOG_ERROR("Source not loaded");
            return false;
        }
        calc_result = fmt::format("{} {} {}", src1, operator_str, src2);
    }

    if (calc_result.empty()) {
        LOG_ERROR("Source not loaded");
        return false;
    }

    variables.store(decoded_inst.opr.dest, calc_result, 0b0001, dest_repeat_offset);
    END_REPEAT()

    reset_repeat_multiplier();

    return true;
}

bool USSETranslatorVisitorGLSL::i8mad(
    Imm2 pred,
    Imm1 cmod1,
    Imm1 skipinv,
    Imm1 nosched,
    Imm2 csel0,
    Imm1 dest_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm1 cmod2,
    Imm3 repeat_count,
    Imm1 saturated,
    Imm1 cmod0,
    Imm1 asel0,
    Imm1 amod2,
    Imm1 amod1,
    Imm1 amod0,
    Imm1 csel1,
    Imm1 csel2,
    Imm1 src0_neg,
    Imm1 src0_bank,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_num,
    Imm7 src0_num,
    Imm7 src1_num,
    Imm7 src2_num) {
    if (!USSETranslatorVisitor::i8mad(pred, cmod1, skipinv, nosched, csel0, dest_bank_ext, end, src1_bank_ext,
            src2_bank_ext, cmod2, repeat_count, saturated, cmod0, asel0, amod2, amod1, amod0, csel1, csel2, src0_neg,
            src0_bank, dest_bank, src1_bank, src2_bank, dest_num, src0_num, src1_num, src2_num)) {
        return false;
    }

    BEGIN_REPEAT(repeat_count);
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    decoded_inst.opr.src0.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    std::string src1_mul = variables.load(decoded_inst.opr.src1, 0b1111, src1_repeat_offset);
    std::string src2_mul = variables.load(decoded_inst.opr.src2, 0b1111, src2_repeat_offset);
    std::string src0_add = variables.load(decoded_inst.opr.src0, 0b1111, src0_repeat_offset);

    std::string final_add = src0_add;

    usse::Swizzle3 add_swizz_rgb = SWIZZLE_CHANNEL_3_DEFAULT;
    bool add_swizz_rgb_src0 = true;

    if ((csel0 != 0) || (asel0 != 0)) {
        switch (csel0) {
        case 0:
            final_add = src0_add + ".xyz";
            break;

        case 1:
            // Use src1 rgb
            final_add = src1_mul + ".xyz";
            break;

        case 2:
            // Use src0 full alpha
            final_add = src0_add + ".www";
            break;

        case 3:
            // Use src1 full alpha
            final_add = src1_mul + ".www";
            break;

        default:
            assert(false);
            break;
        }

        switch (asel0) {
        case 0:
            // Use src0 alpha
            final_add += std::string(", ") + src0_add + ".w";
            break;

        case 1:
            // Use src1 alpha
            final_add += std::string(", ") + src1_mul + ".w";
            break;

        default:
            assert(false);
            break;
        }

        final_add = std::string("uvec4(") + final_add + ")";
    }

    // For some reason there's no FMA for integer
    std::string result = fmt::format("{} * {} {} {}", src1_mul, src2_mul, src0_neg ? "-" : "+", final_add);
    variables.store(decoded_inst.opr.dest, result, 0b1111, dest_repeat_offset);

    decoded_inst.opr.src0.swizzle[0] = add_swizz_rgb[0];
    decoded_inst.opr.src0.swizzle[1] = add_swizz_rgb[1];
    decoded_inst.opr.src0.swizzle[2] = add_swizz_rgb[2];

    END_REPEAT();

    return true;
}

bool USSETranslatorVisitorGLSL::i16mad(
    ShortPredicate pred,
    Imm1 abs,
    Imm1 skipinv,
    Imm1 nosched,
    Imm1 src2_neg,
    Imm1 sel1h_upper8,
    Imm1 dest_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm3 repeat_count,
    Imm2 mode,
    Imm2 src2_format,
    Imm2 src1_format,
    Imm1 sel2h_upper8,
    Imm2 or_shift,
    Imm1 src0_bank,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src0_n,
    Imm7 src1_n,
    Imm7 src2_n) {
    if (!USSETranslatorVisitor::i16mad(pred, abs, skipinv, nosched, src2_neg, sel1h_upper8, dest_bank_ext, end,
            src1_bank_ext, src2_bank_ext, repeat_count, mode, src2_format, src1_format, sel2h_upper8, or_shift,
            src0_bank, dest_bank, src1_bank, src2_bank, dest_n, src0_n, src1_n, src2_n)) {
        return false;
    }

    BEGIN_REPEAT(repeat_count);
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    decoded_inst.opr.src0.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    std::string source0 = variables.load(decoded_inst.opr.src0, 0b11, src0_repeat_offset);
    std::string source1 = variables.load(decoded_inst.opr.src1, decoded_source_mask, src1_repeat_offset);
    std::string source2 = variables.load(decoded_inst.opr.src2, decoded_source_mask_2, src2_repeat_offset);

    bool is_signed = (mode >= 2);
    source1 = fmt::format("{}vec2({})", is_signed ? "i" : "u", source1);
    source2 = fmt::format("{}vec2({})", is_signed ? "i" : "u", source2);

    std::string result = fmt::format("{} * {} + {}", source0, source1, source2);
    variables.store(decoded_inst.opr.dest, result, 0b11, dest_repeat_offset);

    END_REPEAT();

    return true;
}

bool USSETranslatorVisitorGLSL::i32mad(
    ShortPredicate pred,
    Imm1 src0_high,
    Imm1 nosched,
    Imm1 src1_high,
    Imm1 src2_high,
    bool dest_bank_ext,
    Imm1 end,
    bool src1_bank_ext,
    bool src2_bank_ext,
    RepeatCount repeat_count,
    bool is_signed,
    bool is_sat,
    Imm2 src2_type,
    Imm1 src0_bank,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src0_n,
    Imm7 src1_n,
    Imm7 src2_n) {
    if (!USSETranslatorVisitor::i32mad(pred, src0_high, nosched, src1_high, src2_high, dest_bank_ext, end,
            src1_bank_ext, src2_bank_ext, repeat_count, is_signed, is_sat, src2_type, src0_bank,
            dest_bank, src1_bank, src2_bank, dest_n, src0_n, src1_n, src2_n)) {
        return false;
    }

    std::string vsrc0 = variables.load(decoded_inst.opr.src0, decoded_source_mask, 0);
    std::string vsrc1 = variables.load(decoded_inst.opr.src1, decoded_source_mask_2, 0);
    std::string vsrc2 = variables.load(decoded_inst.opr.src2, decoded_source_mask_3, 0);

    std::string result = fmt::format("{} * {} + {}", vsrc0, vsrc1, vsrc2);
    variables.store(decoded_inst.opr.dest, result, 0b1, 0);

    return true;
}

bool USSETranslatorVisitorGLSL::i32mad2(
    ExtPredicate pred,
    Imm1 nosched,
    Imm2 sn,
    bool dest_bank_ext,
    Imm1 end,
    bool src1_bank_ext,
    bool src2_bank_ext,
    bool src0_bank_ext,
    Imm3 count,
    bool is_signed,
    Imm1 negative_src1,
    Imm1 negative_src2,
    Imm1 src0_bank,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src0_n,
    Imm7 src1_n,
    Imm7 src2_n) {
    if (!USSETranslatorVisitor::i32mad2(pred, nosched, sn, dest_bank_ext, end, src1_bank_ext, src2_bank_ext,
            src0_bank_ext, count, is_signed, negative_src1, negative_src2, src0_bank, dest_bank, src1_bank,
            src2_bank, dest_n, src0_n, src1_n, src2_n)) {
        return false;
    }

    std::string vsrc0 = variables.load(decoded_inst.opr.src0, 0b1, 0);
    std::string vsrc1 = variables.load(decoded_inst.opr.src1, 0b1, 0);
    std::string vsrc2 = variables.load(decoded_inst.opr.src2, 0b1, 0);

    std::string result = fmt::format("{} * {} + {}", vsrc0, vsrc1, vsrc2);

    // sn is mysterious argument.
    // These are confirmed by hw testing:
    // - pa = x * y + z (sn = 0) => pa = x * y + z
    // - i = x * y + z (sn = 0) and then pa = x * y + i (sn = 1) => pa = x * y + z
    // - pa = x * y + z (sn = 1) => crash
    // TODO: properly implement this when we get more powerful fuzzer that can handle fpinternal.
    if (sn == 0) {
        variables.store(decoded_inst.opr.dest, result, 0b1, 0);
    } else {
        variables.store(decoded_inst.opr.dest, vsrc2, 0b1, 0);
    }

    return true;
}