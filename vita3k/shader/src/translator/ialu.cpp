
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
#include <shader/translator.h>
#include <shader/types.h>
#include <util/log.h>

namespace shader::usse {
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
    switch (op1) {
    case 0b010: decoded_inst.opcode = op2 ? Opcode::OR : Opcode::AND; break;
    case 0b011: decoded_inst.opcode = Opcode::XOR; break;
    case 0b100: decoded_inst.opcode = op2 ? Opcode::ROL : Opcode::SHL; break;
    case 0b101: decoded_inst.opcode = op2 ? Opcode::ASR : Opcode::SHR; break;
    default: return false;
    }

    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_ext, false, 7, m_second_program);
    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank, src2_ext, false, 7, m_second_program);
    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank, dest_ext, false, 7, m_second_program);

    decoded_inst.opr.src1.type = DataType::UINT32;
    decoded_inst.opr.src2.type = DataType::UINT32;
    decoded_inst.opr.dest.type = DataType::UINT32;

    set_repeat_multiplier(1, 1, 1, 1);

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    bool immediate = src2_ext && decoded_inst.opr.src2.bank == RegisterBank::IMMEDIATE;
    std::uint32_t value = 0;
    if (immediate) {
        value = src2_n | (static_cast<uint32_t>(src2_sel) << 7) | (static_cast<uint32_t>(src2_exth) << 14);
        if (src2_invert) {
            value = ~value;
        }
    }

    LOG_DISASM("{:016x}: {}{} {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(decoded_inst.opcode),
        disasm::operand_to_str(decoded_inst.opr.dest, 0b0001, dest_repeat_offset), disasm::operand_to_str(decoded_inst.opr.src1, 0b0001, src1_repeat_offset),
        immediate ? log_hex(value) : disasm::operand_to_str(decoded_inst.opr.src2, 0b0001, src2_repeat_offset));

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
    decoded_inst.opr.src0 = decode_src0(decoded_inst.opr.src0, src0_num, src0_bank, false, false, 7, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_num, src1_bank, src1_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_num, src2_bank, src2_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_num, dest_bank, dest_bank_ext, false, 7, m_second_program);

    decoded_inst.opr.src0.type = DataType::UINT8;
    decoded_inst.opr.src1.type = DataType::UINT8;
    decoded_inst.opr.src2.type = DataType::UINT8;
    decoded_inst.opr.dest.type = DataType::UINT8;

    decoded_inst.opcode = Opcode::IMA8;

    usse::Swizzle4 src1_swizz = SWIZZLE_CHANNEL_4_DEFAULT;
    usse::Swizzle4 src2_swizz = SWIZZLE_CHANNEL_4_DEFAULT;

    if (csel1) {
        // RGB will be full alpha
        src1_swizz[0] = usse::SwizzleChannel::C_W;
        src1_swizz[1] = usse::SwizzleChannel::C_W;
        src1_swizz[2] = usse::SwizzleChannel::C_W;
    }

    if (csel2) {
        // RGB will be full alpha
        src2_swizz[0] = usse::SwizzleChannel::C_W;
        src2_swizz[1] = usse::SwizzleChannel::C_W;
        src2_swizz[2] = usse::SwizzleChannel::C_W;
    }

    decoded_inst.opr.src1.swizzle = src1_swizz;
    decoded_inst.opr.src2.swizzle = src2_swizz;

    if ((amod0) || (amod1) || (amod2) || (cmod0) || (cmod1) || (cmod2)) {
        LOG_ERROR("Custom modifiers for components not handled!");
    }

    BEGIN_REPEAT(repeat_count);
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    decoded_inst.opr.src0.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    usse::Swizzle3 add_swizz_rgb = SWIZZLE_CHANNEL_3_DEFAULT;
    bool add_swizz_rgb_src0 = true;

    if ((csel0 != 0) || (asel0 != 0)) {
        switch (csel0) {
        case 1:
            add_swizz_rgb_src0 = false;
            break;

        case 2:
            add_swizz_rgb = { usse::SwizzleChannel::C_W, usse::SwizzleChannel::C_W, usse::SwizzleChannel::C_W };
            break;

        case 3:
            add_swizz_rgb = { usse::SwizzleChannel::C_W, usse::SwizzleChannel::C_W, usse::SwizzleChannel::C_W };
            add_swizz_rgb_src0 = false;
            break;

        default:
            assert(false);
            break;
        }
    }

    decoded_inst.opr.src0.swizzle[0] = add_swizz_rgb[0];
    decoded_inst.opr.src0.swizzle[1] = add_swizz_rgb[1];
    decoded_inst.opr.src0.swizzle[2] = add_swizz_rgb[2];

    LOG_DISASM("{:016x}: {}{} {} {} {} {}({}, {})", m_instr, disasm::s_predicate_str(static_cast<usse::ShortPredicate>(pred)),
        disasm::opcode_str(decoded_inst.opcode), disasm::operand_to_str(decoded_inst.opr.dest, 0b1111, dest_repeat_offset),
        disasm::operand_to_str(decoded_inst.opr.src1, 0b1111, src1_repeat_offset),
        disasm::operand_to_str(decoded_inst.opr.src2, 0b1111, src2_repeat_offset),
        src0_neg ? "-" : "", disasm::operand_to_str((add_swizz_rgb_src0 ? decoded_inst.opr.src0 : decoded_inst.opr.src1), 0b0111, src0_repeat_offset),
        disasm::operand_to_str(asel0 ? decoded_inst.opr.src1 : decoded_inst.opr.src0, 0b1000, src0_repeat_offset));

    END_REPEAT();

    return true;
}

bool USSETranslatorVisitor::i8mad2() {
    LOG_DISASM("Unimplmenet Opcode: i8mad2");
    return true;
}

bool USSETranslatorVisitor::i16mad(
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
    if ((mode == 1) || (mode == 3)) {
        LOG_DISASM("Saturation in i16mad not yet supported!");
    }

    decoded_inst.opcode = Opcode::IMA16;
    DataType operate_type = ((mode == 0) || (mode == 1)) ? DataType::UINT16 : DataType::INT16;
    bool is_signed = (mode >= 2);

    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank, dest_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src0 = decode_src0(decoded_inst.opr.src0, src0_n, src0_bank, false, false, 7, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    decoded_inst.opr.dest.type = decoded_inst.opr.src0.type = decoded_inst.opr.src1.type = decoded_inst.opr.src2.type = operate_type;
    decoded_source_mask = 0b1;
    decoded_source_mask_2 = 0b1;
    if (src1_format != 0) {
        if (src1_format == 1) {
            decoded_inst.opr.src1.type = (is_signed ? DataType::INT8 : DataType::UINT8);
        } else {
            LOG_DISASM("Only support selecting non sign-extended 8-bit integer for SRC1!");
        }

        if (sel1h_upper8) {
            decoded_source_mask = 0b10;
        }
    }

    if (src2_format != 0) {
        if (src2_format == 1) {
            decoded_inst.opr.src2.type = (is_signed ? DataType::INT8 : DataType::UINT8);
        } else {
            LOG_DISASM("Only support selecting non sign-extended 8-bit integer for SRC2!");
        }

        if (sel2h_upper8) {
            decoded_source_mask_2 = 0b10;
        }
    }

    BEGIN_REPEAT(repeat_count);
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    LOG_DISASM("{:016x}: {}{} {} {} {} {}", m_instr, disasm::s_predicate_str(pred), "IMAD16", disasm::operand_to_str(decoded_inst.opr.dest, 0b11),
        disasm::operand_to_str(decoded_inst.opr.src0, 0b11), disasm::operand_to_str(decoded_inst.opr.src1, decoded_source_mask) + ((src1_format != 0) ? "-8bits" : ""),
        disasm::operand_to_str(decoded_inst.opr.src2, decoded_source_mask_2) + ((src2_format != 0) ? "-8bits" : ""));

    END_REPEAT();

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
    decoded_inst.opcode = Opcode::IMAD;

    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank, dest_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src0 = decode_src0(decoded_inst.opr.src0, src0_n, src0_bank, false, false, 7, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    const DataType inst_dt = is_signed ? DataType::INT32 : DataType::UINT32;
    const DataType inst_dt_16 = is_signed ? DataType::INT16 : DataType::UINT16;

    decoded_source_mask = 0b1;
    decoded_source_mask_2 = 0b1;
    decoded_source_mask_3 = 0b1;

    decoded_inst.opr.dest.type = inst_dt;
    decoded_inst.opr.src0.type = inst_dt_16;
    decoded_inst.opr.src1.type = inst_dt_16;

    if (src2_type == 2) {
        decoded_inst.opr.src2.type = inst_dt;
    } else {
        decoded_inst.opr.src2.type = inst_dt_16;
    }

    if (src0_high) {
        decoded_source_mask = 0b10;
    }

    if (src1_high) {
        decoded_source_mask_2 = 0b10;
    }

    if (src2_type != 2 && src2_high) {
        decoded_source_mask_3 = 0b10;
    }

    LOG_DISASM("{:016x}: {}{} {} {} {} {}", m_instr, disasm::s_predicate_str(pred), "IMAD2", disasm::operand_to_str(decoded_inst.opr.dest, 0b1),
        disasm::operand_to_str(decoded_inst.opr.src0, decoded_source_mask), disasm::operand_to_str(decoded_inst.opr.src1, decoded_source_mask_2),
        disasm::operand_to_str(decoded_inst.opr.src2, decoded_source_mask_3));

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
    decoded_inst.opcode = Opcode::IMAD;

    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank, dest_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src0 = decode_src0(decoded_inst.opr.src0, src0_n, src0_bank, src0_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    const DataType inst_dt = is_signed ? DataType::INT32 : DataType::UINT32;

    decoded_inst.opr.dest.type = inst_dt;
    decoded_inst.opr.src0.type = inst_dt;
    decoded_inst.opr.src1.type = inst_dt;
    decoded_inst.opr.src2.type = inst_dt;

    if (negative_src1) {
        decoded_inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    if (negative_src2) {
        decoded_inst.opr.src2.flags |= RegisterFlags::Negative;
    }

    LOG_DISASM("{:016x}: {}{} {} {} {} {} [sn={}]", m_instr, disasm::e_predicate_str(pred), "IMAD", disasm::operand_to_str(decoded_inst.opr.dest, 0b1),
        disasm::operand_to_str(decoded_inst.opr.src0, 0b1), disasm::operand_to_str(decoded_inst.opr.src1, 0b1), disasm::operand_to_str(decoded_inst.opr.src2, 0b1), sn);

    return true;
}
} // namespace shader::usse