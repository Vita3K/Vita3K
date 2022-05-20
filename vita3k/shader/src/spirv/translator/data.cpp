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

#include <numeric>

using namespace shader;
using namespace usse;

bool USSETranslatorVisitorSpirv::vmov(
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
    if (!USSETranslatorVisitor::vmov(pred, skipinv, test_bit_2, src0_comp_sel, syncstart, dest_bank_ext, end_or_src0_bank_ext,
            src1_bank_ext, src2_bank_ext, move_type, repeat_count, nosched, move_data_type, test_bit_1, src0_swiz,
            src0_bank_sel, dest_bank_sel, src1_bank_sel, src2_bank_sel, dest_mask, dest_n, src0_n, src1_n, src2_n)) {
        return false;
    }

    const bool is_conditional = (move_type != MoveType::UNCONDITIONAL);
    const bool is_u8_conditional = decoded_inst.opcode == Opcode::VMOVCU8;

    // TODO: adjust dest mask if needed
    CompareMethod compare_method = CompareMethod::NE_ZERO;
    spv::Op compare_op = spv::OpAny;

    if (is_conditional) {
        compare_method = static_cast<CompareMethod>((test_bit_2 << 1) | test_bit_1);

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

    // Recompile

    m_b.setLine(m_recompiler.cur_pc);

    if ((move_data_type == DataType::F16) || (move_data_type == DataType::F32)) {
        set_repeat_multiplier(2, 2, 2, 2);
    } else {
        set_repeat_multiplier(1, 1, 1, 1);
    }

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    spv::Id source_to_compare_with_0 = spv::NoResult;
    spv::Id source_1 = load(decoded_inst.opr.src1, decoded_dest_mask, src1_repeat_offset);
    spv::Id source_2 = spv::NoResult;
    spv::Id result = spv::NoResult;

    if (source_1 == spv::NoResult) {
        LOG_ERROR("Source not Loaded");
        return false;
    }

    if (is_conditional) {
        source_to_compare_with_0 = load(decoded_inst.opr.src0, decoded_dest_mask, src0_repeat_offset);
        source_2 = load(decoded_inst.opr.src2, decoded_dest_mask, src2_repeat_offset);
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

    store(decoded_inst.opr.dest, result, decoded_dest_mask, dest_repeat_offset);

    END_REPEAT()

    reset_repeat_multiplier();
    return true;
}

bool USSETranslatorVisitorSpirv::vpck(
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
    if (!USSETranslatorVisitor::vpck(pred, skipinv, nosched, unknown, syncstart, dest_bank_ext,
            end, src1_bank_ext, src2_bank_ext, repeat_count, src_fmt, dest_fmt, dest_mask, dest_bank_sel,
            src1_bank_sel, src2_bank_sel, dest_n, comp_sel_3, scale, comp_sel_1, comp_sel_2, src1_n,
            comp0_sel_bit1, src2_n, comp_sel_0_bit0)) {
        return false;
    }

    // Recompile
    m_b.setLine(m_recompiler.cur_pc);

    // Doing this extra dest type check for future change in case I'm wrong (pent0)
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

    spv::Id source = load(decoded_inst.opr.src1, dest_mask, src1_repeat_offset);

    if (source == spv::NoResult) {
        LOG_ERROR("Source not loaded");
        return false;
    }

    if (decoded_inst.opr.src2.type != DataType::UNK) {
        Operand src1 = decoded_inst.opr.src1;
        Operand src2 = decoded_inst.opr.src2;
        src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
        src2.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
        spv::Id source1 = load(src1, 0b11, src1_repeat_offset);
        spv::Id source2 = load(src2, 0b11, src2_repeat_offset);
        source = utils::finalize(m_b, source1, source2, decoded_inst.opr.src1.swizzle, 0, dest_mask);
    }

    // source is float destination is int
    if (is_float_data_type(decoded_inst.opr.dest.type) && !is_float_data_type(decoded_inst.opr.src1.type)) {
        source = utils::convert_to_float(m_b, source, decoded_inst.opr.src1.type, scale);
    }

    // source is int destination is float
    if (!is_float_data_type(decoded_inst.opr.dest.type) && is_float_data_type(decoded_inst.opr.src1.type)) {
        source = utils::convert_to_int(m_b, source, decoded_inst.opr.dest.type, scale);
    }

    store(decoded_inst.opr.dest, source, dest_mask, dest_repeat_offset);
    END_REPEAT()

    reset_repeat_multiplier();

    return true;
}

bool USSETranslatorVisitorSpirv::vldst(
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
    if (!USSETranslatorVisitor::vldst(op1, pred, skipinv, nosched, moe_expand, sync_start, cache_ext, src0_bank_ext,
            src1_bank_ext, src2_bank_ext, mask_count, addr_mode, mode, dest_bank_primattr, range_enable, data_type,
            increment_or_decrement, src0_bank, cache_by_pass12, drc_sel, src1_bank, src2_bank, dest_n, src0_n, src1_n,
            src2_n)) {
        return false;
    }

    // TODO:
    // - Store instruction
    // - Post or pre or any increment mode.
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

    // TODO: is source_2 in word or byte? Is it even used at all?
    spv::Id source_0 = load(decoded_inst.opr.src0, 0b1, 0);

    if (decoded_inst.opr.src1.bank == RegisterBank::IMMEDIATE) {
        decoded_inst.opr.src1.num *= get_data_type_size(type_to_ldst);
    }

    spv::Id source_1 = load(decoded_inst.opr.src1, 0b1, 0);
    spv::Id source_2 = load(decoded_inst.opr.src2, 0b1, 0);

    // Seems that if it's indexed by register, offset is in bytes and based on 0x10000?
    // Maybe that's just how the memory map operates. I'm not sure. However the literals on all shader so far is that
    // Another thing is that, when moe expand is not enable, there seems to be 4 bytes added before fetching... No absolute prove.
    // Maybe moe expand means it's not fetching after all? Dunno
    std::uint32_t REG_INDEX_BASE = 0x10000;
    spv::Id reg_index_base_cst = m_b.makeIntConstant(REG_INDEX_BASE);

    if (decoded_inst.opr.src1.bank != shader::usse::RegisterBank::IMMEDIATE) {
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
        store(decoded_inst.opr.dest, src, 0b1);
        decoded_inst.opr.dest.num += 1;
    }

    return true;
}

bool USSETranslatorVisitorSpirv::limm(
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
