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

#include <shader/usse_translator.h>

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/SpvBuilder.h>

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_types.h>
#include <util/log.h>

#include <numeric>

using namespace shader;
using namespace usse;

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
    Instruction inst;

    static const Opcode tb_decode_vmov[] = {
        Opcode::VMOV,
        Opcode::VMOVC,
        Opcode::VMOVCU8,
        Opcode::INVALID,
    };

    inst.opcode = tb_decode_vmov[(Imm3)move_type];
    // TODO: dest mask
    // TODO: flags
    // TODO: test type

    const bool is_double_regs = move_data_type == DataType::C10 || move_data_type == DataType::F16 || move_data_type == DataType::F32;
    const bool is_conditional = (move_type != MoveType::UNCONDITIONAL);
    const bool is_u8_conditional = inst.opcode == Opcode::VMOVCU8;

    // Decode operands
    uint8_t reg_bits = is_double_regs ? 7 : 6;

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, is_double_regs, reg_bits, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, is_double_regs, reg_bits, m_second_program);

    dest_mask = decode_write_mask(inst.opr.dest.bank, dest_mask, move_data_type == DataType::F16);

    // Velocity uses a vec4 table, non-extended, so i assumes type=vec4, extended=false
    inst.opr.src1.swizzle = decode_vec34_swizzle(src0_swiz, false, 2);

    inst.opr.src1.type = move_data_type;
    inst.opr.dest.type = move_data_type;

    // TODO: adjust dest mask if needed
    CompareMethod compare_method = CompareMethod::NE_ZERO;
    spv::Op compare_op = spv::OpAny;

    if (is_conditional) {
        inst.opr.src0.type = is_u8_conditional ? DataType::UINT8 : DataType::F32;
        compare_method = static_cast<CompareMethod>((test_bit_2 << 1) | test_bit_1);
        inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank_sel, end_or_src0_bank_ext, is_double_regs, reg_bits, m_second_program);
        inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, is_double_regs, reg_bits, m_second_program);

        if (src0_comp_sel) {
            inst.opr.src0.swizzle = inst.opr.src1.swizzle;
        }

        inst.opr.src2.swizzle = inst.opr.src1.swizzle;

        switch (compare_method) {
        case CompareMethod::LT_ZERO:
            if (is_u8_conditional)
                compare_op = spv::Op::OpULessThan;
            else
                compare_op = spv::Op::OpFOrdLessThan;
            break;
        case CompareMethod::LTE_ZERO:
            if (is_u8_conditional)
                compare_op = spv::Op::OpULessThanEqual;
            else
                compare_op = spv::Op::OpFOrdLessThanEqual;
            break;
        case CompareMethod::NE_ZERO:
            if (is_u8_conditional)
                compare_op = spv::Op::OpINotEqual;
            else
                compare_op = spv::Op::OpFOrdNotEqual;
            break;
        case CompareMethod::EQ_ZERO:
            if (is_u8_conditional)
                compare_op = spv::Op::OpIEqual;
            else
                compare_op = spv::Op::OpFOrdEqual;
            break;
        }
    }

    if (inst.opr.dest.bank == RegisterBank::SPECIAL || inst.opr.src0.bank == RegisterBank::SPECIAL || inst.opr.src1.bank == RegisterBank::SPECIAL || inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    // Recompile

    m_b.setLine(m_recompiler.cur_pc);

    if ((move_data_type == DataType::F16) || (move_data_type == DataType::F32)) {
        set_repeat_multiplier(2, 2, 2, 2);
    } else {
        set_repeat_multiplier(1, 1, 1, 1);
    }

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(inst, RepeatMode::SLMSI);

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
        conditional_str = fmt::format(" ({} {} vec(0)) ?", disasm::operand_to_str(inst.opr.src0, dest_mask), expr);
    }

    const std::string disasm_str = fmt::format("{:016x}: {}{}.{} {}{} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), disasm::data_type_str(move_data_type),
        disasm::operand_to_str(inst.opr.dest, dest_mask, dest_repeat_offset), conditional_str, disasm::operand_to_str(inst.opr.src1, dest_mask, src1_repeat_offset),
        is_conditional ? fmt::format(": {}", disasm::operand_to_str(inst.opr.src2, dest_mask, src2_repeat_offset)) : "");

    LOG_DISASM(disasm_str);

    spv::Id source_to_compare_with_0 = spv::NoResult;
    spv::Id source_1 = load(inst.opr.src1, dest_mask, src1_repeat_offset);
    spv::Id source_2 = spv::NoResult;
    spv::Id result = spv::NoResult;

    if (source_1 == spv::NoResult) {
        LOG_ERROR("Source not Loaded");
        return false;
    }

    if (is_conditional) {
        source_to_compare_with_0 = load(inst.opr.src0, dest_mask, src0_repeat_offset);
        source_2 = load(inst.opr.src2, dest_mask, src2_repeat_offset);
        spv::Id result_type = m_b.getTypeId(source_2);
        spv::Id v0_comp_type = is_u8_conditional ? m_b.makeUintType(32) : m_b.makeFloatType(32);
        spv::Id v0_type = utils::make_vector_or_scalar_type(m_b, v0_comp_type, m_b.getNumComponents(source_2));
        spv::Id v0 = utils::make_uniform_vector_from_type(m_b, v0_type, 0);

        bool source_2_first = false;

        if (compare_op != spv::OpAny) {
            // Merely do what the instruction does
            // First compare source0 with vector 0
            spv::Id cond_result = m_b.createOp(compare_op, m_b.makeVectorType(m_b.makeBoolType(), m_b.getNumComponents(source_to_compare_with_0)),
                { source_to_compare_with_0, v0 });

            // For each component, if the compare result is true, move the equivalent component from source1 to dest,
            // else the same thing with source2
            // This behavior matches with OpSelect, so use it. Since IMix doesn't exist (really)
            result = m_b.createOp(spv::OpSelect, result_type, { cond_result, source_1, source_2 });
        } else {
            // We optimize the float case. We can make the GPU use native float instructions without touching bool or integers
            // Taking advantage of the mix function: if we use absolute 0 and 1 as the lerp, we got the equivalent of:
            // mix(a, b, c) with c.comp is either 0 or 1 <=> if c.comp == 0 return a else return b.
            switch (compare_method) {
            case CompareMethod::LT_ZERO: {
                // For each component: if source0.comp < 0 return 0 else return 1
                // That means if we use mix, it should be mix(src1, src2, step_result)
                result = m_b.createBuiltinCall(result_type, std_builtins, GLSLstd450Step, { v0, source_to_compare_with_0 });
                source_2_first = false;
                break;
            }

            case CompareMethod::LTE_ZERO: {
                // For each component: if 0 < source0.comp return 0 else return 1
                // Or, if we turn it around: if source0.comp <= 0 return 1 else return 0
                // That means if we use mix, it should be mix(src2, src1, step_result)
                result = m_b.createBuiltinCall(result_type, std_builtins, GLSLstd450Step, { source_to_compare_with_0, v0 });
                source_2_first = true;
                break;
            }

            case CompareMethod::NE_ZERO:
            case CompareMethod::EQ_ZERO: {
                // Taking advantage of the sign and absolute instruction
                // The sign instruction returns 0 if the component equals to 0, else 1 if positive, -1 if negative
                // That means if we absolute the sign result, we got 0 if component equals to 0, else we got 1.
                // src2 will be first for Not equal case.
                result = m_b.createBuiltinCall(result_type, std_builtins, GLSLstd450FSign, { source_to_compare_with_0 });
                result = m_b.createBuiltinCall(result_type, std_builtins, GLSLstd450FAbs, { result });

                if (compare_method == CompareMethod::NE_ZERO) {
                    source_2_first = true;
                }

                break;
            }

            default: {
                LOG_ERROR("Unknown compare method: {}", static_cast<int>(compare_method));
                return false;
            }
            }

            // Mixing!! I'm like a little witch!!
            result = m_b.createBuiltinCall(result_type, std_builtins, GLSLstd450FMix, { source_2_first ? source_2 : source_1, source_2_first ? source_1 : source_2, result });
        }
    } else {
        result = source_1;
    }

    store(inst.opr.dest, result, dest_mask, dest_repeat_offset);

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
    Instruction inst;

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

    inst.opcode = op_table[dest_fmt][src_fmt];

    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode));

    inst.opr.dest.type = dest_data_type_table[dest_fmt];
    inst.opr.src1.type = src_data_type_table[src_fmt];

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, false, 7, m_second_program);

    bool should_src1_dreg = true;

    if (!is_float_data_type(inst.opr.src1.type)) {
        // For non-float source,
        src1_n = comp0_sel_bit1 | (src1_n << 1);
        should_src1_dreg = false;
    }

    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, should_src1_dreg, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, true, 7, m_second_program);

    if (inst.opr.dest.bank == RegisterBank::SPECIAL || inst.opr.src0.bank == RegisterBank::SPECIAL || inst.opr.src1.bank == RegisterBank::SPECIAL || inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    Imm2 comp_sel_0 = comp_sel_0_bit0;

    if (inst.opr.src1.type == DataType::F32)
        comp_sel_0 |= (comp0_sel_bit1 & 1) << 1;
    else
        comp_sel_0 |= (src2_n & 1) << 1;

    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_CAST(comp_sel_0, comp_sel_1, comp_sel_2, comp_sel_3);

    // For some occasions the swizzle needs to cycle from the first components to the first bit that was on in the dest mask.
    bool no_swizzle_cycle_to_mask = (is_float_data_type(inst.opr.src1.type) && is_float_data_type(inst.opr.dest.type))
        || (scale && ((inst.opr.src1.type == DataType::UINT8) || (inst.opr.dest.type == DataType::UINT8)));

    bool should_use_src2 = false;
    constexpr Imm4 CONTIGUOUS_MASKS[] = { 0b1, 0b11, 0b111, 0b1111 };

    if (inst.opr.src1.type == DataType::F32 && inst.opr.src2.bank != RegisterBank::IMMEDIATE) {
        const bool is_two_source_same = inst.opr.src1.num == inst.opr.src2.num && inst.opr.src1.bank == inst.opr.src2.bank;
        const bool is_contiguous = std::any_of(std::begin(CONTIGUOUS_MASKS), std::end(CONTIGUOUS_MASKS), [&dest_mask](const auto mask) { return dest_mask == mask; });
        if (!is_default(inst.opr.src1.swizzle, dest_mask_to_comp_count(dest_mask)) || !is_two_source_same || !is_contiguous) {
            should_use_src2 = true;
        }
    }

    if (!no_swizzle_cycle_to_mask) {
        shader::usse::Swizzle4 swizz_temp = inst.opr.src1.swizzle;
        const bool check_only_two_swizz = (inst.opr.src1.type == DataType::UINT8) || (inst.opr.dest.type == DataType::UINT8);

        int swizz_src_taken = 0;

        // Cycle through swizzle with only one source
        for (int i = 0; i < 4; i++) {
            if (dest_mask & (1 << i)) {
                if (check_only_two_swizz && (dest_mask == 0b1111)) {
                    inst.opr.src1.swizzle[i] = swizz_temp[i % 2];
                } else {
                    inst.opr.src1.swizzle[i] = swizz_temp[swizz_src_taken++];
                }
            }
        }
    }

    // Recompile
    m_b.setLine(m_recompiler.cur_pc);

    // Doing this extra dest type check for future change in case I'm wrong (pent0)
    if (is_integer_data_type(inst.opr.dest.type)) {
        if (is_float_data_type(inst.opr.src1.type)) {
            set_repeat_multiplier(1, 2, 2, 1);
        } else {
            set_repeat_multiplier(1, 1, 1, 1);
        }
    } else {
        if (is_float_data_type(inst.opr.src1.type)) {
            set_repeat_multiplier(1, 2, 2, 1);
        } else {
            set_repeat_multiplier(1, 1, 1, 1);
        }
    }

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(inst, RepeatMode::SLMSI);

    if (should_use_src2) {
        // TODO correctly log
        LOG_DISASM("{} {} ({} {}) [{}]", disasm_str, disasm::operand_to_str(inst.opr.dest, dest_mask, dest_repeat_offset),
            disasm::operand_to_str(inst.opr.src1, dest_mask, src1_repeat_offset),
            disasm::operand_to_str(inst.opr.src2, 0b1111, src2_repeat_offset), scale ? "scale" : "noscale");
    } else {
        LOG_DISASM("{} {} {} [{}]", disasm_str, disasm::operand_to_str(inst.opr.dest, dest_mask, dest_repeat_offset),
            disasm::operand_to_str(inst.opr.src1, dest_mask, src1_repeat_offset), scale ? "scale" : "noscale");
    }

    spv::Id source = load(inst.opr.src1, dest_mask, src1_repeat_offset);

    if (source == spv::NoResult) {
        LOG_ERROR("Source not loaded");
        return false;
    }

    if (should_use_src2) {
        Operand src1 = inst.opr.src1;
        Operand src2 = inst.opr.src2;
        src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
        src2.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
        spv::Id source1 = load(src1, 0b11, src1_repeat_offset);
        spv::Id source2 = load(src2, 0b11, src2_repeat_offset);
        source = utils::finalize(m_b, source1, source2, inst.opr.src1.swizzle, 0, dest_mask);
    }

    // source is float destination is int
    if (is_float_data_type(inst.opr.dest.type) && !is_float_data_type(inst.opr.src1.type)) {
        source = utils::convert_to_float(m_b, source, inst.opr.src1.type, scale);
    }

    // source is int destination is float
    if (!is_float_data_type(inst.opr.dest.type) && is_float_data_type(inst.opr.src1.type)) {
        source = utils::convert_to_int(m_b, source, inst.opr.dest.type, scale);
    }

    store(inst.opr.dest, source, dest_mask, dest_repeat_offset);
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

    Instruction inst;
    inst.opcode = Opcode::LDR;

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

    Operand to_store;

    if (is_translating_secondary_program()) {
        to_store.bank = RegisterBank::SECATTR;
    } else {
        if (dest_bank_primattr) {
            to_store.bank = RegisterBank::PRIMATTR;
        } else {
            to_store.bank = RegisterBank::TEMP;
        }
    }

    to_store.num = dest_n;
    to_store.type = DataType::F32;

    inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank, src0_bank_ext, false, 7, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    inst.opr.src0.type = DataType::INT32;
    inst.opr.src1.type = DataType::INT32;
    inst.opr.src2.type = DataType::INT32;

    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode));
    LOG_DISASM("{} {} ({} + {} + {}) [{} bytes]", disasm_str, disasm::operand_to_str(to_store, 0b1, 0),
        disasm::operand_to_str(inst.opr.src0, 0b1, 0),
        disasm::operand_to_str(inst.opr.src1, 0b1, 0), disasm::operand_to_str(inst.opr.src2, 0b1, 0), total_bytes_fo_fetch);

    // TODO: is source_2 in word or byte? Is it even used at all?
    spv::Id source_0 = load(inst.opr.src0, 0b1, 0);

    if (inst.opr.src1.bank == RegisterBank::IMMEDIATE) {
        inst.opr.src1.num *= get_data_type_size(type_to_ldst);
    }

    spv::Id source_1 = load(inst.opr.src1, 0b1, 0);
    spv::Id source_2 = load(inst.opr.src2, 0b1, 0);

    // Seems that if it's indexed by register, offset is in bytes and based on 0x10000?
    // Maybe that's just how the memory map operates. I'm not sure. However the literals on all shader so far is that
    // Another thing is that, when moe expand is not enable, there seems to be 4 bytes added before fetching... No absolute prove.
    // Maybe moe expand means it's not fetching after all? Dunno
    std::uint32_t REG_INDEX_BASE = 0x10000;
    spv::Id reg_index_base_cst = m_b.makeIntConstant(REG_INDEX_BASE);

    if (inst.opr.src1.bank != shader::usse::RegisterBank::IMMEDIATE) {
        source_1 = m_b.createBinOp(spv::OpISub, m_b.getTypeId(source_1), source_1, reg_index_base_cst);
    }

    spv::Id i32_type = m_b.makeIntType(32);
    spv::Id base = m_b.createBinOp(spv::OpIAdd, i32_type, source_0, source_1);
    base = m_b.createBinOp(spv::OpIAdd, i32_type, base, source_2);

    if (!moe_expand) {
        base = m_b.createBinOp(spv::OpIAdd, i32_type, base, m_b.makeIntConstant(4));
    }

    for (int i = 0; i < total_bytes_fo_fetch / 4; ++i) {
        spv::Id offset = m_b.createBinOp(spv::OpIAdd, m_b.makeIntType(32), base, m_b.makeIntConstant(4 * i));
        spv::Id src = utils::fetch_memory(m_b, m_spirv_params, m_util_funcs, offset);
        store(to_store, src, 0b1);
        to_store.num += 1;
    }

    return true;
}
