// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

#include <shader/decoder_helpers.h>
#include <shader/disasm.h>
#include <shader/types.h>
#include <util/log.h>

using namespace shader;
using namespace usse;

bool USSETranslatorVisitorSpirv::vbw(
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

    spv::Id src2 = 0;
    if (immediate) {
        value = src2_n | (static_cast<uint32_t>(src2_sel) << 7) | (static_cast<uint32_t>(src2_exth) << 14);
        src2 = m_b.makeUintConstant(src2_invert ? ~value : value);
    } else {
        src2 = load(decoded_inst.opr.src2, 0b0001, src2_repeat_offset);

        if (src2 == spv::NoResult) {
            LOG_ERROR("Source 2 not loaded");
            return false;
        }

        if (src2_invert) {
            src2 = m_b.createUnaryOp(spv::Op::OpNot, type_ui32, src2);
        }
    }

    spv::Id result;

    spv::Op operation;
    switch (decoded_inst.opcode) {
    case Opcode::OR: operation = spv::Op::OpBitwiseOr; break;
    case Opcode::AND: operation = spv::Op::OpBitwiseAnd; break;
    case Opcode::XOR: operation = spv::Op::OpBitwiseXor; break;
    case Opcode::ROL:
        LOG_WARN("Bitwise Rotate Left operation unsupported.");
        return false; // TODO: SPIRV doesn't seem to have a rotate left operation!
    case Opcode::ASR: operation = spv::Op::OpShiftRightArithmetic; break;
    case Opcode::SHL: operation = spv::Op::OpShiftLeftLogical; break;
    case Opcode::SHR: operation = spv::Op::OpShiftRightLogical; break;
    default: return false;
    }
    // optimisation. (any OR 0 || any XOR 0 || any AND 0xFFFFFFFF) -> assign
    bool is_const = m_b.getOpCode(src2) == spv::Op::OpConstant;
    auto const_val = is_const ? m_b.getConstantScalar(src2) : 1; // default value is intentionally non zero
    if ((operation == spv::Op::OpBitwiseOr || operation == spv::Op::OpBitwiseXor) && is_const && const_val == 0
        || operation == spv::Op::OpBitwiseAnd && is_const && const_val == std::numeric_limits<decltype(const_val)>::max()) {
        decoded_inst.opr.src1.type = DataType::F32;
        decoded_inst.opr.dest.type = DataType::F32;
        result = load(decoded_inst.opr.src1, 0b0001, src1_repeat_offset);
        if (result == spv::NoResult) {
            LOG_ERROR("Source not loaded");
            return false;
        }
    } else {
        spv::Id src1 = load(decoded_inst.opr.src1, 0b0001, src1_repeat_offset);
        if (src1 == spv::NoResult) {
            LOG_ERROR("Source not loaded");
            return false;
        }
        result = m_b.createBinOp(operation, type_ui32, src1, src2);
        if (m_b.isFloatType(m_b.getTypeId(src2))) {
            result = m_b.createUnaryOp(spv::Op::OpBitcast, type_f32, src2);
        }
    }

    store(decoded_inst.opr.dest, result, 0b0001, dest_repeat_offset);
    END_REPEAT()

    reset_repeat_multiplier();

    return true;
}

bool USSETranslatorVisitorSpirv::i8mad(
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
    RepeatCount repeat_count,
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

    spv::Id src1_mul = load(decoded_inst.opr.src1, 0b1111, src1_repeat_offset);
    spv::Id src2_mul = load(decoded_inst.opr.src2, 0b1111, src2_repeat_offset);
    spv::Id src0_add = load(decoded_inst.opr.src0, 0b1111, src0_repeat_offset);

    spv::Id final_add = src0_add;

    usse::Swizzle3 add_swizz_rgb = SWIZZLE_CHANNEL_3_DEFAULT;
    bool add_swizz_rgb_src0 = true;

    if ((csel0 != 0) || (asel0 != 0)) {
        // We build source0 (the add component in this loop)
        // Using OpVectorShuffle to construct the final one.
        std::vector<spv::Id> shuffle_ops = { src0_add, src1_mul };

        switch (csel0) {
        case 0:
            shuffle_ops.insert(shuffle_ops.end(), { 0, 1, 2 });
            break;

        case 1:
            // Use src1 rgb
            shuffle_ops.insert(shuffle_ops.end(), { 4, 5, 6 });
            add_swizz_rgb_src0 = false;

            break;

        case 2:
            // Use src0 full alpha
            shuffle_ops.insert(shuffle_ops.end(), { 3, 3, 3 });
            add_swizz_rgb = { usse::SwizzleChannel::C_W, usse::SwizzleChannel::C_W, usse::SwizzleChannel::C_W };

            break;

        case 3:
            // Use src1 full alpha
            shuffle_ops.insert(shuffle_ops.end(), { 7, 7, 7 });
            add_swizz_rgb = { usse::SwizzleChannel::C_W, usse::SwizzleChannel::C_W, usse::SwizzleChannel::C_W };
            add_swizz_rgb_src0 = false;

            break;

        default:
            assert(false);
            break;
        }

        switch (asel0) {
        case 0:
            // Use src0 alpha
            shuffle_ops.push_back(3);
            break;

        case 1:
            // Use src1 alpha
            shuffle_ops.push_back(7);
            break;

        default:
            assert(false);
            break;
        }

        final_add = m_b.createOp(spv::OpVectorShuffle, m_b.getTypeId(src0_add), shuffle_ops);
    }

    spv::Id result = m_b.createBinOp(spv::OpIMul, m_b.getTypeId(src1_mul), src1_mul, src2_mul);
    result = m_b.createBinOp(src0_neg ? spv::OpISub : spv::OpIAdd, m_b.getTypeId(src1_mul), result, final_add);

    store(decoded_inst.opr.dest, result, 0b1111, dest_repeat_offset);

    decoded_inst.opr.src0.swizzle[0] = add_swizz_rgb[0];
    decoded_inst.opr.src0.swizzle[1] = add_swizz_rgb[1];
    decoded_inst.opr.src0.swizzle[2] = add_swizz_rgb[2];

    END_REPEAT();

    return true;
}

bool USSETranslatorVisitorSpirv::i16mad(
    ShortPredicate pred,
    Imm1 abs,
    bool skipinv,
    bool nosched,
    Imm1 src2_neg,
    Imm1 sel1h_upper8,
    Imm1 dest_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    RepeatCount repeat_count,
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
    spv::Id source0 = load(decoded_inst.opr.src0, 0b11, src0_repeat_offset);
    spv::Id source1 = load(decoded_inst.opr.src1, decoded_source_mask, src1_repeat_offset);
    spv::Id source2 = load(decoded_inst.opr.src2, decoded_source_mask_2, src2_repeat_offset);

    spv::Id source0_type = m_b.getTypeId(source0);
    source1 = m_b.createCompositeConstruct(source0_type, { source1, source1 });
    source2 = m_b.createCompositeConstruct(source0_type, { source2, source2 });

    auto mul_result = m_b.createBinOp(spv::OpIMul, source0_type, source0, source1);
    auto add_result = m_b.createBinOp(spv::OpIAdd, source0_type, mul_result, source2);

    if (add_result != spv::NoResult) {
        store(decoded_inst.opr.dest, add_result, 0b11, dest_repeat_offset);
    }

    END_REPEAT();

    return true;
}

bool USSETranslatorVisitorSpirv::i32mad(
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

    spv::Id vsrc0 = load(decoded_inst.opr.src0, decoded_source_mask, 0);
    spv::Id vsrc1 = load(decoded_inst.opr.src1, decoded_source_mask_2, 0);
    spv::Id vsrc2 = load(decoded_inst.opr.src2, decoded_source_mask_3, 0);

    auto mul_result = m_b.createBinOp(spv::OpIMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpIAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    if (add_result != spv::NoResult) {
        store(decoded_inst.opr.dest, add_result, 0b1, 0);
    }

    return true;
}

bool USSETranslatorVisitorSpirv::i32mad2(
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

    spv::Id vsrc0 = load(decoded_inst.opr.src0, 0b1, 0);
    spv::Id vsrc1 = load(decoded_inst.opr.src1, 0b1, 0);
    spv::Id vsrc2 = load(decoded_inst.opr.src2, 0b1, 0);

    auto mul_result = m_b.createBinOp(spv::OpIMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpIAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    // sn is mysterious argument.
    // These are confirmed by hw testing:
    // - pa = x * y + z (sn = 0) => pa = x * y + z
    // - i = x * y + z (sn = 0) and then pa = x * y + i (sn = 1) => pa = x * y + z
    // - pa = x * y + z (sn = 1) => crash
    // TODO: properly implement this when we get more powerful fuzzer that can handle fpinternal.
    if (sn == 0) {
        store(decoded_inst.opr.dest, add_result, 0b1, 0);
    } else {
        store(decoded_inst.opr.dest, vsrc2, 0b1, 0);
    }

    return true;
}
