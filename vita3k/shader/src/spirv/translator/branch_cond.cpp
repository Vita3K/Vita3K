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

#include <shader/decoder_helpers.h>
#include <shader/disasm.h>
#include <shader/spirv/translator.h>
#include <shader/types.h>
#include <util/log.h>

using namespace shader;
using namespace usse;

spv::Id USSETranslatorVisitorSpirv::vtst_impl(Instruction inst, ExtPredicate pred, int zero_test, int sign_test, Imm4 load_mask, bool mask) {
    // Usually we would expect this to have a compare behavior
    // Comparision is done by subtracting the first src by the second src, and compare the result value.
    // We currently optimize for that case first
    const DataType load_data_type = inst.opr.src1.type;
    const DataType store_data_type = inst.opr.dest.type;

    static const spv::Op tb_comp_ops[3][2][4] = {
        { { spv::OpFOrdNotEqual,
              spv::OpFOrdLessThan,
              spv::OpFOrdGreaterThan,
              spv::OpAll },
            { spv::OpFOrdEqual,
                spv::OpFOrdLessThanEqual,
                spv::OpFOrdGreaterThanEqual,
                spv::OpAll } },
        { { spv::OpINotEqual,
              spv::OpSLessThan,
              spv::OpSGreaterThan,
              spv::OpAll },
            { spv::OpIEqual,
                spv::OpSLessThanEqual,
                spv::OpSGreaterThanEqual,
                spv::OpAll } },
        { { spv::OpINotEqual,
              spv::OpULessThan,
              spv::OpUGreaterThan,
              spv::OpAll },
            { spv::OpIEqual,
                spv::OpULessThanEqual,
                spv::OpUGreaterThanEqual,
                spv::OpAll } },
    };

    // Load our compares
    spv::Id lhs = spv::NoResult;
    spv::Id rhs = spv::NoResult;

    const spv::Id pred_type = utils::make_vector_or_scalar_type(m_b, m_b.makeBoolType(), mask ? 4 : 1);
    spv::Id pred_result = utils::make_uniform_vector_from_type(m_b, m_b.makeBoolType(), true);

    // Zero test number:
    // 0 - alway pass
    // 1 - zero
    // 2 - non-zero
    const bool compare_include_equal = (zero_test == 1);

    // Sign test number

    int index_tb_comp = 0;
    if (is_signed_integer_data_type(load_data_type)) {
        index_tb_comp = 1;
    } else if (is_unsigned_integer_data_type(load_data_type)) {
        index_tb_comp = 2;
    }

    const spv::Op used_comp_op = tb_comp_ops[index_tb_comp][compare_include_equal][sign_test];

    // Optimize this case. Alternative name is CMP.
    const char *tb_comp_str[2][4] = {
        { "ne",
            "lt",
            "gt",
            "inv" },
        { "equal",
            "le",
            "ge",
            "inv" }
    };

    const char *used_comp_str = tb_comp_str[compare_include_equal][sign_test];

    m_b.setLine(m_recompiler.cur_pc);

    if (is_sub_opcode(inst.opcode)) {
        if (mask) {
            LOG_DISASM("{:016x}: {}{}.{}.{}.{} {} {} {}", m_instr, disasm::e_predicate_str(pred), "CMPMSK", used_comp_str, disasm::data_type_str(store_data_type),
                disasm::data_type_str(load_data_type), disasm::operand_to_str(inst.opr.dest, 0b1111), disasm::operand_to_str(inst.opr.src1, load_mask),
                disasm::operand_to_str(inst.opr.src2, load_mask));
        } else {
            LOG_DISASM("{:016x}: {}{}.{}.{} p{} {} {}", m_instr, disasm::e_predicate_str(pred), "CMP", used_comp_str, disasm::data_type_str(load_data_type),
                inst.opr.dest.num, disasm::operand_to_str(inst.opr.src1, load_mask), disasm::operand_to_str(inst.opr.src2, load_mask));
        }

        lhs = load(inst.opr.src1, load_mask);
        rhs = load(inst.opr.src2, load_mask);
    } else {
        if (mask) {
            LOG_DISASM("{:016x}: {}{}.{}zero.{}.{} {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), used_comp_str, disasm::data_type_str(store_data_type),
                disasm::data_type_str(load_data_type), disasm::operand_to_str(inst.opr.dest, 0b1111), disasm::operand_to_str(inst.opr.src1, load_mask), disasm::operand_to_str(inst.opr.src2, load_mask));
        } else {
            LOG_DISASM("{:016x}: {}{}.{}zero.{} p{} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), used_comp_str, disasm::data_type_str(load_data_type),
                inst.opr.dest.num, disasm::operand_to_str(inst.opr.src1, load_mask), disasm::operand_to_str(inst.opr.src2, load_mask));
        }

        lhs = do_alu_op(inst, load_mask, mask ? mask : 0b1);

        const spv::Id c0_type = utils::make_vector_or_scalar_type(m_b, m_b.makeFloatType(32), mask ? 4 : 1);
        spv::Id c0 = utils::make_uniform_vector_from_type(m_b, c0_type, 0.0f);

        if (is_signed_integer_data_type(load_data_type)) {
            c0 = m_b.makeIntConstant(0);
        } else if (is_unsigned_integer_data_type(load_data_type)) {
            c0 = m_b.makeUintConstant(0);
        }

        rhs = c0;
    }

    if (lhs == spv::NoResult || rhs == spv::NoResult) {
        LOG_ERROR("Source not loaded (lhs: {}, rhs: {})", lhs, rhs);
        return spv::NoResult;
    }

    return m_b.createOp(used_comp_op, pred_type, { lhs, rhs });
}

bool USSETranslatorVisitorSpirv::vtst(
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 onceonly,
    Imm1 syncstart,
    Imm1 dest_ext,
    Imm1 src1_neg,
    Imm1 src1_ext,
    Imm1 src2_ext,
    Imm1 prec,
    Imm1 src2_vscomp,
    RepeatCount rpt_count,
    Imm2 sign_test,
    Imm2 zero_test,
    Imm1 test_crcomb_and,
    Imm3 chan_cc,
    Imm2 pdst_n,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm1 test_wben,
    Imm2 alu_sel,
    Imm4 alu_op,
    Imm7 src1_n,
    Imm7 src2_n) {
    if (!USSETranslatorVisitor::vtst(pred, skipinv, onceonly, syncstart, dest_ext, src1_neg, src1_ext, src2_ext,
            prec, src2_vscomp, rpt_count, sign_test, zero_test, test_crcomb_and, chan_cc, pdst_n, dest_bank,
            src1_bank, src2_bank, dest_n, test_wben, alu_sel, alu_op, src1_n, src2_n)) {
        return false;
    }

    const spv::Id pred_result = vtst_impl(decoded_inst, pred, zero_test, sign_test, decoded_source_mask, false);
    store(decoded_inst.opr.dest, pred_result);

    return true;
}

bool USSETranslatorVisitorSpirv::vtstmsk(
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 onceonly,
    Imm1 syncstart,
    Imm1 dest_ext,
    Imm1 test_flag_2,
    Imm1 src1_ext,
    Imm1 src2_ext,
    Imm1 prec,
    Imm1 src2_vscomp,
    RepeatCount rpt_count,
    Imm2 sign_test,
    Imm2 zero_test,
    Imm1 test_crcomb_and,
    Imm2 tst_mask_type,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm1 test_wben,
    Imm2 alu_sel,
    Imm4 alu_op,
    Imm7 src1_n,
    Imm7 src2_n) {
    if (!USSETranslatorVisitor::vtstmsk(pred, skipinv, onceonly, syncstart, dest_ext, test_flag_2, src1_ext,
            src2_ext, prec, src2_vscomp, rpt_count, sign_test, zero_test, test_crcomb_and, tst_mask_type,
            dest_bank, src1_bank, src2_bank, dest_n, test_wben, alu_sel, alu_op, src1_n, src2_n)) {
        return false;
    }

    spv::Id pred_result = vtst_impl(decoded_inst, pred, zero_test, sign_test, 0b1111, true);

    spv::Id float_v4 = utils::make_vector_or_scalar_type(m_b, m_b.makeFloatType(32), 4);
    spv::Id uint_v4 = utils::make_vector_or_scalar_type(m_b, m_b.makeUintType(32), 4);
    spv::Id scaler;
    switch (decoded_inst.opr.dest.type) {
    case DataType::UINT8:
        // OpSelect doesn't work UConvert doesn't work in glsl transpiler
        pred_result = m_b.createUnaryOp(spv::OpFConvert, float_v4, pred_result);
        pred_result = m_b.createUnaryOp(spv::OpConvertFToU, uint_v4, pred_result);
        scaler = m_b.makeIntConstant(0xFF);
        pred_result = m_b.createBinOp(spv::OpIMul, uint_v4, pred_result, scaler);
        break;
    case DataType::F16:
    case DataType::F32:
        pred_result = m_b.createUnaryOp(spv::OpFConvert, float_v4, pred_result);
        break;
    }

    store(decoded_inst.opr.dest, pred_result);

    return true;
}
