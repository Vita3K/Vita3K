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
bool USSETranslatorVisitor::vmov(
    ExtPredicate pred,
    bool skipinv,
    Imm1 test_bit_2,
    Imm1 src0_comp_sel,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 end_or_src0_bank_ext,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    MoveType move_type,
    RepeatCount repeat_count,
    bool nosched,
    DataType move_data_type,
    Imm1 test_bit_1,
    Imm4 src0_swiz,
    Imm1 src0_bank_sel,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm2 src2_bank_sel,
    Imm4 dest_mask,
    Imm6 dest_n,
    Imm6 src0_n,
    Imm6 src1_n,
    Imm6 src2_n) {
    static const Opcode tb_decode_vmov[] = {
        Opcode::VMOV,
        Opcode::VMOVC,
        Opcode::VMOVCU8,
        Opcode::INVALID,
    };

    decoded_inst.opcode = tb_decode_vmov[(Imm3)move_type];
    // TODO: dest mask
    // TODO: flags
    // TODO: test type

    const bool is_double_regs = move_data_type == DataType::C10 || move_data_type == DataType::F16 || move_data_type == DataType::F32;
    const bool is_conditional = (move_type != MoveType::UNCONDITIONAL);
    const bool is_u8_conditional = decoded_inst.opcode == Opcode::VMOVCU8;

    // Decode operands
    uint8_t reg_bits = is_double_regs ? 7 : 6;

    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, is_double_regs, reg_bits, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, is_double_regs, reg_bits, m_second_program);

    decoded_dest_mask = decode_write_mask(decoded_inst.opr.dest.bank, dest_mask, move_data_type == DataType::F16);

    // Velocity uses a vec4 table, non-extended, so i assumes type=vec4, extended=false
    decoded_inst.opr.src1.swizzle = decode_vec34_swizzle(src0_swiz, false, 2);

    decoded_inst.opr.src1.type = move_data_type;
    decoded_inst.opr.dest.type = move_data_type;

    if (is_conditional) {
        decoded_inst.opr.src0.type = is_u8_conditional ? DataType::UINT8 : DataType::F32;
        decoded_inst.opr.src0 = decode_src0(decoded_inst.opr.src0, src0_n, src0_bank_sel, end_or_src0_bank_ext, is_double_regs, reg_bits, m_second_program);
        decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, is_double_regs, reg_bits, m_second_program);

        if (src0_comp_sel) {
            decoded_inst.opr.src0.swizzle = decoded_inst.opr.src1.swizzle;
        }

        decoded_inst.opr.src2.swizzle = decoded_inst.opr.src1.swizzle;
        decoded_inst.opr.src2.type = move_data_type;
    }

    if (decoded_inst.opr.dest.bank == RegisterBank::SPECIAL || decoded_inst.opr.src0.bank == RegisterBank::SPECIAL || decoded_inst.opr.src1.bank == RegisterBank::SPECIAL || decoded_inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    if ((move_data_type == DataType::F16) || (move_data_type == DataType::F32)) {
        set_repeat_multiplier(2, 2, 2, 2);
    } else {
        set_repeat_multiplier(1, 1, 1, 1);
    }

    CompareMethod compare_method = CompareMethod::NE_ZERO;

    if (is_conditional) {
        compare_method = static_cast<CompareMethod>((test_bit_2 << 1) | test_bit_1);
    }

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    std::string conditional_str;
    if (is_conditional) {
        std::string expr;
        switch (compare_method) {
        case CompareMethod::LT_ZERO:
            expr = "<";
            break;
        case CompareMethod::LTE_ZERO:
            expr = "<=";
            break;
        case CompareMethod::EQ_ZERO:
            expr = "==";
            break;
        case CompareMethod::NE_ZERO:
            expr = "!=";
            break;
        }
        conditional_str = fmt::format(" ({} {} vec(0)) ?", disasm::operand_to_str(decoded_inst.opr.src0, decoded_dest_mask), expr);
    }

    const std::string disasm_str = fmt::format("{:016x}: {}{}.{} {}{} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(decoded_inst.opcode), disasm::data_type_str(move_data_type),
        disasm::operand_to_str(decoded_inst.opr.dest, decoded_dest_mask, dest_repeat_offset), conditional_str, disasm::operand_to_str(decoded_inst.opr.src1, decoded_dest_mask, src1_repeat_offset),
        is_conditional ? fmt::format(": {}", disasm::operand_to_str(decoded_inst.opr.src2, decoded_dest_mask, src2_repeat_offset)) : "");

    LOG_DISASM(disasm_str);
    END_REPEAT()

    reset_repeat_multiplier();
    return true;
}

bool USSETranslatorVisitor::vpck(
    ExtPredicate pred,
    bool skipinv,
    bool nosched,
    Imm1 unknown,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    RepeatCount repeat_count,
    Imm3 src_fmt,
    Imm3 dest_fmt,
    Imm4 dest_mask,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm2 src2_bank_sel,
    Imm7 dest_n,
    Imm2 comp_sel_3,
    Imm1 scale,
    Imm2 comp_sel_1,
    Imm2 comp_sel_2,
    Imm6 src1_n,
    Imm1 comp0_sel_bit1,
    Imm6 src2_n,
    Imm1 comp_sel_0_bit0) {
    // TODO: There are some combinations that are invalid.
    const DataType dest_data_type_table[] = {
        DataType::UINT8,
        DataType::INT8,
        DataType::O8,
        DataType::UINT16,
        DataType::INT16,
        DataType::F16,
        DataType::F32,
        DataType::C10
    };

    const DataType src_data_type_table[] = {
        DataType::UINT8,
        DataType::INT8,
        DataType::O8,
        DataType::UINT16,
        DataType::INT16,
        DataType::F16,
        DataType::F32,
        DataType::C10
    };

    const Opcode op_table[][static_cast<int>(DataType::TOTAL_TYPE)] = {
        { Opcode::VPCKU8U8,
            Opcode::VPCKU8S8,
            Opcode::VPCKU8O8,
            Opcode::VPCKU8U16,
            Opcode::VPCKU8S16,
            Opcode::VPCKU8F16,
            Opcode::VPCKU8F32,
            Opcode::INVALID },
        { Opcode::VPCKS8U8,
            Opcode::VPCKS8S8,
            Opcode::VPCKS8O8,
            Opcode::VPCKS8U16,
            Opcode::VPCKS8S16,
            Opcode::VPCKS8F16,
            Opcode::VPCKS8F32,
            Opcode::INVALID },
        { Opcode::VPCKO8U8,
            Opcode::VPCKO8S8,
            Opcode::VPCKO8O8,
            Opcode::VPCKO8U16,
            Opcode::VPCKO8S16,
            Opcode::VPCKO8F16,
            Opcode::VPCKO8F32,
            Opcode::INVALID },
        { Opcode::VPCKU16U8,
            Opcode::VPCKU16S8,
            Opcode::VPCKU16O8,
            Opcode::VPCKU16U16,
            Opcode::VPCKU16S16,
            Opcode::VPCKU16F16,
            Opcode::VPCKU16F32,
            Opcode::INVALID },
        { Opcode::VPCKS16U8,
            Opcode::VPCKS16S8,
            Opcode::VPCKS16O8,
            Opcode::VPCKS16U16,
            Opcode::VPCKS16S16,
            Opcode::VPCKS16F16,
            Opcode::VPCKS16F32,
            Opcode::INVALID },
        { Opcode::VPCKF16U8,
            Opcode::VPCKF16S8,
            Opcode::VPCKF16O8,
            Opcode::VPCKF16U16,
            Opcode::VPCKF16S16,
            Opcode::VPCKF16F16,
            Opcode::VPCKF16F32,
            Opcode::VPCKF16C10 },
        { Opcode::VPCKF32U8,
            Opcode::VPCKF32S8,
            Opcode::VPCKF32O8,
            Opcode::VPCKF32U16,
            Opcode::VPCKF32S16,
            Opcode::VPCKF32F16,
            Opcode::VPCKF32F32,
            Opcode::VPCKF32C10 },
        { Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::VPCKC10F16,
            Opcode::VPCKC10F32,
            Opcode::VPCKC10C10 }
    };

    decoded_inst.opcode = op_table[dest_fmt][src_fmt];

    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(decoded_inst.opcode));

    decoded_inst.opr.dest.type = dest_data_type_table[dest_fmt];
    decoded_inst.opr.src1.type = src_data_type_table[src_fmt];
    decoded_inst.opr.src2.type = decoded_inst.opr.src1.type;

    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, false, 7, m_second_program);

    bool should_src1_dreg = true;

    if (!is_float_data_type(decoded_inst.opr.src1.type)) {
        // For non-float source,
        src1_n = comp0_sel_bit1 | (src1_n << 1);
        should_src1_dreg = false;
    }

    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, should_src1_dreg, 7, m_second_program);
    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, true, 7, m_second_program);

    if (decoded_inst.opr.dest.bank == RegisterBank::SPECIAL || decoded_inst.opr.src0.bank == RegisterBank::SPECIAL || decoded_inst.opr.src1.bank == RegisterBank::SPECIAL || decoded_inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    Imm2 comp_sel_0 = comp_sel_0_bit0;

    if (decoded_inst.opr.src1.type == DataType::F32)
        comp_sel_0 |= (comp0_sel_bit1 & 1) << 1;
    else
        comp_sel_0 |= (src2_n & 1) << 1;

    decoded_inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_CAST(comp_sel_0, comp_sel_1, comp_sel_2, comp_sel_3);

    // For some occasions the swizzle needs to cycle from the first components to the first bit that was on in the dest mask.
    bool no_swizzle_cycle_to_mask = (is_float_data_type(decoded_inst.opr.src1.type) && is_float_data_type(decoded_inst.opr.dest.type))
        || (scale && ((decoded_inst.opr.src1.type == DataType::UINT8) || (decoded_inst.opr.dest.type == DataType::UINT8)));

    bool should_use_src2 = false;
    constexpr Imm4 CONTIGUOUS_MASKS[] = { 0b1, 0b11, 0b111, 0b1111 };

    if (decoded_inst.opr.src1.type == DataType::F32 && decoded_inst.opr.src2.bank != RegisterBank::IMMEDIATE) {
        const bool is_two_source_same = decoded_inst.opr.src1.num == decoded_inst.opr.src2.num && decoded_inst.opr.src1.bank == decoded_inst.opr.src2.bank;
        const bool is_contiguous = std::any_of(std::begin(CONTIGUOUS_MASKS), std::end(CONTIGUOUS_MASKS), [&dest_mask](const auto mask) { return dest_mask == mask; });
        if (!is_default(decoded_inst.opr.src1.swizzle, dest_mask_to_comp_count(dest_mask)) || !is_two_source_same || !is_contiguous) {
            should_use_src2 = true;
        } else {
            decoded_inst.opr.src2.type = DataType::UNK;
        }
    } else {
        decoded_inst.opr.src2.type = DataType::UNK;
    }

    if (!no_swizzle_cycle_to_mask) {
        shader::usse::Swizzle4 swizz_temp = decoded_inst.opr.src1.swizzle;
        const bool check_only_two_swizz = (decoded_inst.opr.src1.type == DataType::UINT8) || (decoded_inst.opr.dest.type == DataType::UINT8);

        int swizz_src_taken = 0;

        // Cycle through swizzle with only one source
        for (int i = 0; i < 4; i++) {
            if (dest_mask & (1 << i)) {
                if (check_only_two_swizz && (dest_mask == 0b1111)) {
                    decoded_inst.opr.src1.swizzle[i] = swizz_temp[i % 2];
                } else {
                    decoded_inst.opr.src1.swizzle[i] = swizz_temp[swizz_src_taken++];
                }
            }
        }
    }

    if (is_integer_data_type(decoded_inst.opr.dest.type)) {
        if (is_float_data_type(decoded_inst.opr.src1.type)) {
            set_repeat_multiplier(1, 2, 2, 1);
        } else {
            set_repeat_multiplier(1, 1, 1, 1);
        }
    } else {
        if (is_float_data_type(decoded_inst.opr.src1.type)) {
            set_repeat_multiplier(1, 2, 2, 1);
        } else {
            set_repeat_multiplier(1, 1, 1, 1);
        }
    }

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    if (should_use_src2) {
        // TODO correctly log
        LOG_DISASM("{} {} ({} {}) [{}]", disasm_str, disasm::operand_to_str(decoded_inst.opr.dest, dest_mask, dest_repeat_offset),
            disasm::operand_to_str(decoded_inst.opr.src1, dest_mask, src1_repeat_offset),
            disasm::operand_to_str(decoded_inst.opr.src2, 0b1111, src2_repeat_offset), scale ? "scale" : "noscale");
    } else {
        LOG_DISASM("{} {} {} [{}]", disasm_str, disasm::operand_to_str(decoded_inst.opr.dest, dest_mask, dest_repeat_offset),
            disasm::operand_to_str(decoded_inst.opr.src1, dest_mask, src1_repeat_offset), scale ? "scale" : "noscale");
    }

    END_REPEAT()
    reset_repeat_multiplier();

    return true;
}

bool USSETranslatorVisitor::vldst(
    Imm2 op1,
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 nosched,
    Imm1 moe_expand,
    Imm1 sync_start,
    Imm1 cache_ext,
    Imm1 src0_bank_ext,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm4 mask_count,
    Imm2 addr_mode,
    Imm2 mode,
    Imm1 dest_bank_primattr,
    Imm1 range_enable,
    Imm2 data_type,
    Imm1 increment_or_decrement,
    Imm1 src0_bank,
    Imm1 cache_by_pass12,
    Imm1 drc_sel,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src0_n,
    Imm7 src1_n,
    Imm7 src2_n) {
    // TODO:
    // - Store instruction
    // - Post or pre or any increment mode.
    decoded_inst.opcode = Opcode::LDR;
    DataType type_to_ldst = DataType::UNK;

    switch (data_type) {
    case 0:
        type_to_ldst = DataType::F32;
        break;

    case 1:
        type_to_ldst = DataType::F16;
        break;

    case 2:
        type_to_ldst = DataType::C10;
        break;

    default:
        break;
    }

    const int total_number_to_fetch = mask_count + 1;
    const int total_bytes_fo_fetch = get_data_type_size(type_to_ldst) * total_number_to_fetch;

    decoded_inst.opr.src0 = decode_src0(decoded_inst.opr.src0, src0_n, src0_bank, src0_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    decoded_inst.opr.src0.type = DataType::INT32;
    decoded_inst.opr.src1.type = DataType::INT32;
    decoded_inst.opr.src2.type = DataType::INT32;

    if (is_translating_secondary_program()) {
        decoded_inst.opr.dest.bank = RegisterBank::SECATTR;
    } else {
        if (dest_bank_primattr) {
            decoded_inst.opr.dest.bank = RegisterBank::PRIMATTR;
        } else {
            decoded_inst.opr.dest.bank = RegisterBank::TEMP;
        }
    }

    decoded_inst.opr.dest.num = dest_n;
    decoded_inst.opr.dest.type = DataType::F32;

    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(decoded_inst.opcode));
    LOG_DISASM("{} {} ({} + {} + {}) [{} bytes]", disasm_str, disasm::operand_to_str(decoded_inst.opr.dest, 0b1, 0),
        disasm::operand_to_str(decoded_inst.opr.src0, 0b1, 0),
        disasm::operand_to_str(decoded_inst.opr.src1, 0b1, 0), disasm::operand_to_str(decoded_inst.opr.src2, 0b1, 0), total_bytes_fo_fetch);

    return true;
}
} // namespace shader::usse