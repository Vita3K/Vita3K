// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

    const bool is_double_regs = move_data_type == DataType::C10 || move_data_type == DataType::F16 || move_data_type == DataType::F32;
    const bool is_conditional = (move_type != MoveType::UNCONDITIONAL);
    const bool is_u8_conditional = inst.opcode == Opcode::VMOVCU8;

    // Decode operands
    uint8_t reg_bits = is_double_regs ? 7 : 6;

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, is_double_regs, reg_bits, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, is_double_regs, reg_bits, m_second_program);

    if (move_data_type == DataType::F16 || move_data_type == DataType::F32)
        dest_mask = decode_write_mask(inst.opr.dest.bank, dest_mask, move_data_type == DataType::F16);
    else if (!is_double_regs)
        // only floating point moves are vectorized
        dest_mask = 0b1;

    // Velocity uses a vec4 table, non-extended, so i assumes type=vec4, extended=false
    inst.opr.src1.swizzle = is_double_regs ? decode_vec34_swizzle(src0_swiz, false, 2) : (Swizzle4 SWIZZLE_CHANNEL_4_DEFAULT);

    inst.opr.src1.type = move_data_type;
    inst.opr.dest.type = move_data_type;

    // TODO: adjust dest mask if needed
    CompareMethod compare_method = CompareMethod::NE_ZERO;
    spv::Op compare_op = spv::OpAny;

    const DataType test_type = is_u8_conditional ? DataType::UINT8 : move_data_type;
    const bool is_test_signed = is_signed_integer_data_type(test_type);
    const bool is_test_unsigned = is_unsigned_integer_data_type(test_type);
    const bool is_test_integer = is_test_signed || is_test_unsigned;

    if (is_conditional) {
        compare_method = static_cast<CompareMethod>((test_bit_2 << 1) | test_bit_1);
        inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank_sel, end_or_src0_bank_ext, is_double_regs, reg_bits, m_second_program);
        inst.opr.src0.type = test_type;
        inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, is_double_regs, reg_bits, m_second_program);
        inst.opr.src2.type = move_data_type;

        if (src0_comp_sel) {
            inst.opr.src0.swizzle = inst.opr.src1.swizzle;
        }

        inst.opr.src2.swizzle = inst.opr.src1.swizzle;

        switch (compare_method) {
        case CompareMethod::LT_ZERO:
            if (is_test_unsigned)
                compare_op = spv::Op::OpULessThan;
            else if (is_test_signed)
                compare_op = spv::Op::OpSLessThan;
            else
                compare_op = spv::Op::OpFOrdLessThan;
            break;
        case CompareMethod::LTE_ZERO:
            if (is_test_unsigned)
                compare_op = spv::Op::OpULessThanEqual;
            else if (is_test_signed)
                compare_op = spv::Op::OpSLessThanEqual;
            else
                compare_op = spv::Op::OpFOrdLessThanEqual;
            break;
        case CompareMethod::NE_ZERO:
            if (is_test_integer)
                compare_op = spv::Op::OpINotEqual;
            else
                compare_op = spv::Op::OpFOrdNotEqual;
            break;
        case CompareMethod::EQ_ZERO:
            if (is_test_integer)
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

    LOG_DISASM("{}", disasm_str);

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
        spv::Id v0_comp_type = is_test_unsigned ? m_b.makeUintType(32) : (is_test_signed ? m_b.makeIntType(32) : m_b.makeFloatType(32));
        spv::Id v0_type = utils::make_vector_or_scalar_type(m_b, v0_comp_type, m_b.getNumComponents(source_2));
        spv::Id v0 = utils::make_uniform_vector_from_type(m_b, v0_type, 0);

        bool source_2_first = false;

        if (compare_op != spv::OpAny) {
            // Merely do what the instruction does
            // First compare source0 with vector 0
            spv::Id cond_result = m_b.createOp(compare_op, utils::make_vector_or_scalar_type(m_b, m_b.makeBoolType(), m_b.getNumComponents(source_to_compare_with_0)),
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
        source = utils::finalize(m_b, source1, source2, inst.opr.src1.swizzle, m_b.makeIntConstant(0), dest_mask);
    }

    // source is int destination is float
    if (is_float_data_type(inst.opr.dest.type) && !is_float_data_type(inst.opr.src1.type)) {
        source = utils::convert_to_float(m_b, source, inst.opr.src1.type, scale);
    }

    // source is float destination is int
    if (!is_float_data_type(inst.opr.dest.type) && is_float_data_type(inst.opr.src1.type)) {
        source = utils::convert_to_int(m_b, m_util_funcs, source, inst.opr.dest.type, scale);
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
    // - Post or pre or any increment mode.

    Instruction inst;
    switch (op1) {
    case 1:
        inst.opcode = Opcode::LDR;
        break;
    case 2:
        inst.opcode = Opcode::STR;
        break;
    default:
        LOG_ERROR("Unknown load/store operation {}", op1);
        return true;
    }
    const bool is_store = inst.opcode == Opcode::STR;
    DataType type_to_ldst = DataType::UNK;

    switch (data_type) {
    case 0:
        type_to_ldst = DataType::F32;
        break;

    case 1:
        type_to_ldst = DataType::INT16;
        break;

    case 2:
        type_to_ldst = DataType::INT8;
        break;

    default:
        break;
    }

    const int total_number_to_fetch = mask_count + 1;
    const int total_bytes_fo_fetch = get_data_type_size(type_to_ldst) * total_number_to_fetch;

    inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank, src0_bank_ext, false, 7, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);

    inst.opr.src0.type = DataType::INT32;
    inst.opr.src1.type = DataType::INT32;
    inst.opr.src2.type = DataType::INT32;

    Operand to_store;
    if (is_store) {
        inst.opr.src2.type = type_to_ldst;
        to_store = inst.opr.src2;
    } else {
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
        if (m_features.support_memory_mapping)
            to_store.type = type_to_ldst;
        else
            to_store.type = DataType::F32;
    }

    if (inst.opr.src1.bank == RegisterBank::IMMEDIATE) {
        inst.opr.src1.num *= get_data_type_size(type_to_ldst);
    }

    // right now proper repeat is implemented only for the store operation
    const int repeat_count = is_store ? mask_count : 0;
    set_repeat_multiplier(1, 1, 1, 1);
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(inst, RepeatMode::SLMSI)

    const int current_bytes_to_fetch = is_store ? 4 : total_bytes_fo_fetch;
    const int current_number_to_fetch = is_store ? 1 : total_number_to_fetch;

    const int to_store_offset = is_store ? src2_repeat_offset : 0;
    const int src0_offset = is_store ? src0_repeat_offset : 0;
    const int src1_offset = is_store ? dest_repeat_offset : 0;
    const int src2_offset = 0; // not used when storing

    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode));
    LOG_DISASM("{} {} ({} + {} + {}) [{} bytes]", disasm_str, disasm::operand_to_str(to_store, 0b1, to_store_offset),
        disasm::operand_to_str(inst.opr.src0, 0b1, src0_offset),
        disasm::operand_to_str(inst.opr.src1, 0b1, src1_offset), is_store ? "0" : disasm::operand_to_str(inst.opr.src2, 0b1, src2_offset), current_bytes_to_fetch);

    // check if we handle this literal or texture read
    auto check_for_literal_texture_read = [&]() {
        if (mask_count > 0) {
            LOG_ERROR("Unimplemented literal buffer access with repeat");
            return false;
        }
        if (to_store.type != DataType::F32) {
            LOG_ERROR("Unimplemented non-f32 literal buffer access");
            return false;
        }
        if (inst.opr.src1.bank != RegisterBank::IMMEDIATE || inst.opr.src2.bank != RegisterBank::IMMEDIATE) {
            LOG_ERROR("Unimplemented non-immediate literal buffer access");
            return false;
        }
        if (is_store) {
            LOG_ERROR("Unhandled literal buffer store");
            return false;
        }

        return true;
    };

    if (inst.opr.src0.bank == RegisterBank::SECATTR && inst.opr.src0.num == m_spirv_params.texture_buffer_sa_offset) {
        // We are reading the texture buffer

        if (!check_for_literal_texture_read())
            return true;

        int offset = (m_spirv_params.texture_buffer_base + inst.opr.src1.num + inst.opr.src2.num) / sizeof(uint32_t);
        // we store the texture index in the first texture register, we don't do anything with the other 3
        if (offset % 4 != 0)
            continue;

        to_store.type = DataType::INT32;
        store(to_store, m_b.makeIntConstant(offset / 4), 0b1);
        continue;
    } else if (inst.opr.src0.bank == RegisterBank::SECATTR && inst.opr.src0.num == m_spirv_params.literal_buffer_sa_offset) {
        // We are reading the literal buffer

        if (!check_for_literal_texture_read())
            return true;

        int offset = m_spirv_params.literal_buffer_base + inst.opr.src1.num + inst.opr.src2.num;
        const uint8_t *literal_buffer = m_program.literal_buffer_data();
        const float literal = *reinterpret_cast<const float *>(literal_buffer + offset);

        store(to_store, m_b.makeFloatConstant(literal), 0b1);
        continue;
    }

    spv::Id source_0 = load(inst.opr.src0, 0b1, src0_offset);
    spv::Id source_1 = load(inst.opr.src1, 0b1, src1_offset);

    // are we using the sa register containing the thread buffer address ?
    const bool is_thread_buffer_access = inst.opr.src0.bank == RegisterBank::SECATTR && inst.opr.src0.num == m_spirv_params.thread_buffer_sa_offset;

    // Seems that if it's indexed by register, offset is in bytes and based on 0x10000?
    // Maybe that's just how the memory map operates. I'm not sure. However the literals on all shader so far is that
    // Another thing is that, when moe expand is not enable, there seems to be 4 bytes added before fetching... No absolute prove.
    // Maybe moe expand means it's not fetching after all? Dunno
    // also for the thread buffer, this value is 128 times bigger
    uint32_t REG_INDEX_BASE = is_thread_buffer_access ? 0x1000000 : 0x10000;
    spv::Id reg_index_base_cst = m_b.makeIntConstant(REG_INDEX_BASE);
    spv::Id i32_type = m_b.makeIntType(32);

    if (inst.opr.src1.bank != shader::usse::RegisterBank::IMMEDIATE) {
        source_1 = m_b.createBinOp(spv::OpISub, m_b.getTypeId(source_1), source_1, reg_index_base_cst);
    }

    if (!moe_expand) {
        source_1 = m_b.createBinOp(spv::OpIAdd, i32_type, source_1, m_b.makeIntConstant(4));
    }

    if (!is_store) {
        spv::Id source_2 = load(inst.opr.src2, 0b1, src2_offset);
        source_1 = m_b.createBinOp(spv::OpIAdd, i32_type, source_1, source_2);
    }

    if (is_thread_buffer_access) {
        // We are reading the thread buffer

        // first some checks
        if (mask_count > 0) {
            LOG_ERROR("Unimplemented thread buffer access with repeat");
            return true;
        }
        if (to_store.type != DataType::F32) {
            LOG_ERROR("Unimplemented non-f32 thread buffer access");
            return true;
        }

        if (m_spirv_params.thread_buffer_base != 0)
            source_1 = m_b.createBinOp(spv::OpIAdd, i32_type, source_1, m_spirv_params.thread_buffer_base);

        // get the index in the float array
        spv::Id index = m_b.createBinOp(spv::OpShiftRightLogical, i32_type, source_1, m_b.makeUintConstant(2));
        spv::Id float_ptr = utils::create_access_chain(m_b, spv::StorageClassPrivate, m_spirv_params.thread_buffer, { index });
        if (is_store) {
            spv::Id value = load(to_store, 0b1);
            m_b.createStore(value, float_ptr);
        } else {
            spv::Id value = m_b.createLoad(float_ptr, spv::NoPrecision);
            store(to_store, value, 0b1);
        }
        continue;
    }

    spv::Id base = m_b.createBinOp(spv::OpIAdd, i32_type, source_0, source_1);

    if (m_features.support_memory_mapping) {
        utils::buffer_address_access(m_b, m_spirv_params, m_util_funcs, m_features, to_store, to_store_offset, base, get_data_type_size(type_to_ldst), current_number_to_fetch, -1, is_store);
    } else {
        if (is_store) {
            LOG_ERROR("Store opcode is not supported without memory mapping");
            return true;
        }

        for (int i = 0; i < total_bytes_fo_fetch / 4; ++i) {
            spv::Id offset = m_b.createBinOp(spv::OpIAdd, m_b.makeIntType(32), base, m_b.makeIntConstant(4 * i));
            spv::Id src = utils::fetch_memory(m_b, m_spirv_params, m_util_funcs, offset);
            store(to_store, src, 0b1);
            to_store.num += 1;
        }
    }

    END_REPEAT()
    reset_repeat_multiplier();

    return true;
}

bool USSETranslatorVisitor::limm(
    bool skipinv,
    bool nosched,
    bool dest_bank_ext,
    bool end,
    Imm6 imm_value_bits26to31,
    ExtPredicate pred,
    Imm5 imm_value_bits21to25,
    Imm2 dest_bank,
    Imm7 dest_num,
    Imm21 imm_value_first_21bits) {
    Instruction inst;
    inst.opcode = Opcode::MOV;

    std::uint32_t imm_value = imm_value_first_21bits | (imm_value_bits21to25 << 21) | (imm_value_bits26to31 << 26);
    spv::Id const_imm_id = m_b.makeUintConstant(imm_value);

    inst.dest_mask = 0b1;
    inst.opr.dest = decode_dest(inst.opr.dest, dest_num, dest_bank, dest_bank_ext, false, 7, m_second_program);
    inst.opr.dest.type = DataType::UINT32;

    const std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode));
    LOG_DISASM("{} {} #0x{:X}", disasm_str, disasm::operand_to_str(inst.opr.dest, 0b1, 0), imm_value);

    store(inst.opr.dest, const_imm_id, 0b1);
    return true;
}
