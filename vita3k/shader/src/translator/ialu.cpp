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

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(inst, RepeatMode::SLMSI);

    spv::Id src1 = load(inst.opr.src1, 0b0001, src1_repeat_offset / 2);
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
        src2 = load(inst.opr.src2, 0b0001, src2_repeat_offset / 2);

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

    store(inst.opr.dest, result, 0b0001, dest_repeat_offset / 2);

    LOG_DISASM("{:016x}: {}{} {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode),
        disasm::operand_to_str(inst.opr.dest, 0b0001, dest_repeat_offset / 2), disasm::operand_to_str(inst.opr.src1, 0b0001, src1_repeat_offset / 2),
        immediate ? log_hex(value) : disasm::operand_to_str(inst.opr.src2, 0b0001, src2_repeat_offset / 2));

    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::i8mad() {
    LOG_DISASM("Unimplmenet Opcode: i8mad");
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

    inst.opr.dest.type = inst_dt;
    inst.opr.src0.type = inst_dt;
    inst.opr.src1.type = inst_dt;
    inst.opr.src2.type = inst_dt;

    spv::Id vsrc0 = load(inst.opr.src0, 0b1, 0);
    spv::Id vsrc1 = load(inst.opr.src1, 0b1, 0);
    spv::Id vsrc2 = load(inst.opr.src2, 0b1, 0);

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