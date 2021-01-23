// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

#include <shader/usse_translator.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

using namespace shader;
using namespace usse;

bool USSETranslatorVisitor::vbw(
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
    Instruction inst;

    switch (op1) {
    case 0b010: inst.opcode = op2 ? Opcode::OR : Opcode::AND; break;
    case 0b011: inst.opcode = Opcode::XOR; break;
    case 0b100: inst.opcode = op2 ? Opcode::ROL : Opcode::SHL; break;
    case 0b101: inst.opcode = op2 ? Opcode::ASR : Opcode::SHR; break;
    default: return false;
    }

    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_ext, false, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_ext, false, 7, m_second_program);

    inst.opr.src1.type = DataType::UINT32;
    inst.opr.src2.type = DataType::UINT32;
    inst.opr.dest.type = DataType::UINT32;

    set_repeat_multiplier(1, 1, 1, 1);

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(inst, RepeatMode::SLMSI);

    spv::Id src1 = load(inst.opr.src1, 0b0001, src1_repeat_offset);
    spv::Id src2 = 0;

    if (src1 == spv::NoResult) {
        LOG_ERROR("Source not loaded");
        return false;
    }

    bool immediate = src2_ext && inst.opr.src2.bank == RegisterBank::IMMEDIATE;
    uint32_t value = 0;

    if (src2_rot) {
        LOG_WARN("Bitwise Rotations are unsupported.");
        return false;
    }
    if (immediate) {
        value = src2_n | (static_cast<uint32_t>(src2_sel) << 7) | (static_cast<uint32_t>(src2_exth) << 14);
        src2 = m_b.makeUintConstant(src2_invert ? ~value : value);
    } else {
        src2 = load(inst.opr.src2, 0b0001, src2_repeat_offset);

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
    switch (inst.opcode) {
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

    result = m_b.createBinOp(operation, type_ui32, src1, src2);
    if (m_b.isFloatType(m_b.getTypeId(src2))) {
        result = m_b.createUnaryOp(spv::Op::OpBitcast, type_f32, src2);
    }

    store(inst.opr.dest, result, 0b0001, dest_repeat_offset);

    LOG_DISASM("{:016x}: {}{} {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode),
        disasm::operand_to_str(inst.opr.dest, 0b0001, dest_repeat_offset), disasm::operand_to_str(inst.opr.src1, 0b0001, src1_repeat_offset),
        immediate ? log_hex(value) : disasm::operand_to_str(inst.opr.src2, 0b0001, src2_repeat_offset));

    END_REPEAT()

    reset_repeat_multiplier();

    return true;
}

bool USSETranslatorVisitor::i8mad(
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
    Instruction inst;

    inst.opr.src0 = decode_src0(inst.opr.src0, src0_num, src0_bank, false, false, 7, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_num, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_num, src2_bank, src2_bank_ext, false, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_num, dest_bank, dest_bank_ext, false, 7, m_second_program);

    inst.opr.src0.type = DataType::UINT8;
    inst.opr.src1.type = DataType::UINT8;
    inst.opr.src2.type = DataType::UINT8;
    inst.opr.dest.type = DataType::UINT8;

    inst.opcode = Opcode::IMA8;

    usse::Swizzle4 src1_swizz = SWIZZLE_CHANNEL_4_DEFAULT;
    usse::Swizzle4 src2_swizz = SWIZZLE_CHANNEL_4_DEFAULT;

    if (csel1) {
        // RGB will be full alpha
        src1_swizz[0] = usse::SwizzleChannel::_W;
        src1_swizz[1] = usse::SwizzleChannel::_W;
        src1_swizz[2] = usse::SwizzleChannel::_W;
    }

    if (csel2) {
        // RGB will be full alpha
        src2_swizz[0] = usse::SwizzleChannel::_W;
        src2_swizz[1] = usse::SwizzleChannel::_W;
        src2_swizz[2] = usse::SwizzleChannel::_W;
    }

    inst.opr.src1.swizzle = src1_swizz;
    inst.opr.src2.swizzle = src2_swizz;

    if ((amod0) || (amod1) || (amod2) || (cmod0) || (cmod1) || (cmod2)) {
        LOG_ERROR("Custom modifiers for components not handled!");
    }

    BEGIN_REPEAT(repeat_count);
    GET_REPEAT(inst, RepeatMode::SLMSI);

    inst.opr.src0.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    spv::Id src1_mul = load(inst.opr.src1, 0b1111, src1_repeat_offset);
    spv::Id src2_mul = load(inst.opr.src2, 0b1111, src2_repeat_offset);
    spv::Id src0_add = load(inst.opr.src0, 0b1111, src0_repeat_offset);

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
            add_swizz_rgb = { usse::SwizzleChannel::_W, usse::SwizzleChannel::_W, usse::SwizzleChannel::_W };

            break;

        case 3:
            // Use src1 full alpha
            shuffle_ops.insert(shuffle_ops.end(), { 7, 7, 7 });
            add_swizz_rgb = { usse::SwizzleChannel::_W, usse::SwizzleChannel::_W, usse::SwizzleChannel::_W };
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
    result = m_b.createBinOp(src0_neg ? spv::OpISub : spv::OpIAdd, m_b.getTypeId(src1_mul), result, src0_add);

    store(inst.opr.dest, result, 0b1111, dest_repeat_offset);

    inst.opr.src0.swizzle[0] = add_swizz_rgb[0];
    inst.opr.src0.swizzle[1] = add_swizz_rgb[1];
    inst.opr.src0.swizzle[2] = add_swizz_rgb[2];

    LOG_DISASM("{:016x}: {}{} {} {} {} {}({}, {})", m_instr, disasm::s_predicate_str(static_cast<usse::ShortPredicate>(pred)),
        disasm::opcode_str(inst.opcode), disasm::operand_to_str(inst.opr.dest, 0b1111, dest_repeat_offset),
        disasm::operand_to_str(inst.opr.src1, 0b1111, src1_repeat_offset),
        disasm::operand_to_str(inst.opr.src2, 0b1111, src2_repeat_offset),
        src0_neg ? "-" : "", disasm::operand_to_str((add_swizz_rgb_src0 ? inst.opr.src0 : inst.opr.src1), 0b0111, src0_repeat_offset),
        disasm::operand_to_str(asel0 ? inst.opr.src1 : inst.opr.src0, 0b1000, src0_repeat_offset));

    END_REPEAT();

    return true;
}

bool USSETranslatorVisitor::i8mad2() {
    LOG_DISASM("Unimplmenet Opcode: i8mad2");
    return true;
}

bool USSETranslatorVisitor::i16mad() {
    LOG_DISASM("Unimplmenet Opcode: i16mad");
    return true;
}

bool USSETranslatorVisitor::i32mad(
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
    // TODO: high and low modifier
    // TODO: more data types for src2
    Instruction inst;
    inst.opcode = Opcode::IMAD;

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_bank_ext, false, 7, m_second_program);
    inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank, false, false, 7, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    const DataType inst_dt = is_signed ? DataType::INT32 : DataType::UINT32;
    const DataType inst_dt_16 = is_signed ? DataType::INT16 : DataType::UINT16;

    shader::usse::Imm4 src0_mask = 0b1;
    shader::usse::Imm4 src1_mask = 0b1;
    shader::usse::Imm4 src2_mask = 0b1;

    inst.opr.dest.type = inst_dt;
    inst.opr.src0.type = inst_dt_16;
    inst.opr.src1.type = inst_dt_16;

    if (src2_type == 2) {
        inst.opr.src2.type = inst_dt;
    } else {
        inst.opr.src2.type = inst_dt_16;
    }

    if (src0_high) {
        src0_mask = 0b10;
    }

    if (src1_high) {
        src1_mask = 0b10;
    }

    if (src2_type != 2 && src2_high) {
        src2_mask = 0b10;
    }

    spv::Id vsrc0 = load(inst.opr.src0, src0_mask, 0);
    spv::Id vsrc1 = load(inst.opr.src1, src1_mask, 0);
    spv::Id vsrc2 = load(inst.opr.src2, src2_mask, 0);

    auto mul_result = m_b.createBinOp(spv::OpIMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpIAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    if (add_result != spv::NoResult) {
        store(inst.opr.dest, add_result, 0b1, 0);
    }

    LOG_DISASM("{:016x}: {}{} {} {} {} {}", m_instr, disasm::s_predicate_str(pred), "IMAD2", disasm::operand_to_str(inst.opr.dest, 0b1),
        disasm::operand_to_str(inst.opr.src0, 0b1), disasm::operand_to_str(inst.opr.src1, 0b1), disasm::operand_to_str(inst.opr.src2, 0b1));
    return true;
}

bool USSETranslatorVisitor::i32mad2(
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
    Instruction inst;
    inst.opcode = Opcode::IMAD;

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_bank_ext, false, 7, m_second_program);
    inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank, src0_bank_ext, false, 7, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    const DataType inst_dt = is_signed ? DataType::INT32 : DataType::UINT32;

    inst.opr.dest.type = inst_dt;
    inst.opr.src0.type = inst_dt;
    inst.opr.src1.type = inst_dt;
    inst.opr.src2.type = inst_dt;

    if (negative_src1) {
        inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    if (negative_src2) {
        inst.opr.src2.flags |= RegisterFlags::Negative;
    }

    spv::Id vsrc0 = load(inst.opr.src0, 0b1, 0);
    spv::Id vsrc1 = load(inst.opr.src1, 0b1, 0);
    spv::Id vsrc2 = load(inst.opr.src2, 0b1, 0);

    auto mul_result = m_b.createBinOp(spv::OpIMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpIAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    // sn is mysterious argument.
    // These are confirmed by hw testing:
    // - pa = x * y + z (sn = 0) => pa = x * y + z
    // - i = x * y + z (sn = 0) and then pa = x * y + i (sn = 1) => pa = x * y + z
    // - pa = x * y + z (sn = 1) => crash
    // TODO: properly implement this when we get more powerful fuzzer that can handle fpinternal.
    if (sn == 0) {
        store(inst.opr.dest, add_result, 0b1, 0);
    } else {
        store(inst.opr.dest, vsrc2, 0b1, 0);
    }

    LOG_DISASM("{:016x}: {}{} {} {} {} {} [sn={}]", m_instr, disasm::e_predicate_str(pred), "IMAD", disasm::operand_to_str(inst.opr.dest, 0b1),
        disasm::operand_to_str(inst.opr.src0, 0b1), disasm::operand_to_str(inst.opr.src1, 0b1), disasm::operand_to_str(inst.opr.src2, 0b1), sn);

    return true;
}