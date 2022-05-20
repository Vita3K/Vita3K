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
#include <shader/glsl/code_writer.h>
#include <shader/glsl/params.h>
#include <shader/glsl/translator.h>
#include <shader/types.h>

#include <util/log.h>

using namespace shader;
using namespace usse;
using namespace glsl;

std::string USSETranslatorVisitorGLSL::vtst_impl(Instruction inst, ExtPredicate pred, int zero_test, int sign_test, Imm4 load_mask, bool mask) {
    const DataType load_data_type = inst.opr.src1.type;
    const DataType store_data_type = inst.opr.dest.type;

    static const std::string compare_functions[2][3] = {
        { "notEqual,",
            "lessThan",
            "greaterThan" },
        { "equal",
            "lessThan",
            "greaterThan" }
    };

    static const std::string compare_ops[2][3] = {
        { "!=",
            "<",
            ">" },
        { "==",
            "<=",
            ">=" }
    };

    // Zero test number:
    // 0 - alway pass
    // 1 - zero
    // 2 - non-zero
    const bool compare_include_equal = (zero_test == 1);

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
    std::size_t num_comp_count = dest_mask_to_comp_count(load_mask);

    std::string lhs;
    std::string rhs;

    if (is_sub_opcode(inst.opcode)) {
        if (mask) {
            LOG_DISASM("{:016x}: {}{}.{}.{}.{} {} {} {}", m_instr, disasm::e_predicate_str(pred), "CMPMSK", used_comp_str, disasm::data_type_str(store_data_type),
                disasm::data_type_str(load_data_type), disasm::operand_to_str(inst.opr.dest, 0b1111), disasm::operand_to_str(inst.opr.src1, load_mask),
                disasm::operand_to_str(inst.opr.src2, load_mask));
        } else {
            LOG_DISASM("{:016x}: {}{}.{}.{} p{} {} {}", m_instr, disasm::e_predicate_str(pred), "CMP", used_comp_str, disasm::data_type_str(load_data_type),
                inst.opr.dest.num, disasm::operand_to_str(inst.opr.src1, load_mask), disasm::operand_to_str(inst.opr.src2, load_mask));
        }

        lhs = variables.load(inst.opr.src1, load_mask, 0);
        rhs = variables.load(inst.opr.src2, load_mask, 0);
    } else {
        if (mask) {
            LOG_DISASM("{:016x}: {}{}.{}zero.{}.{} {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), used_comp_str, disasm::data_type_str(store_data_type),
                disasm::data_type_str(load_data_type), disasm::operand_to_str(inst.opr.dest, 0b1111), disasm::operand_to_str(inst.opr.src1, load_mask), disasm::operand_to_str(inst.opr.src2, load_mask));
        } else {
            LOG_DISASM("{:016x}: {}{}.{}zero.{} p{} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), used_comp_str, disasm::data_type_str(load_data_type),
                inst.opr.dest.num, disasm::operand_to_str(inst.opr.src1, load_mask), disasm::operand_to_str(inst.opr.src2, load_mask));
        }

        lhs = do_alu_op(inst, load_mask, mask ? mask : 0b1);
        if (num_comp_count > 1) {
            if (is_unsigned_integer_data_type(load_data_type)) {
                rhs = fmt::format("uvec{}(0)", num_comp_count);
            } else if (is_signed_integer_data_type(load_data_type)) {
                rhs = fmt::format("ivec{}(0)", num_comp_count);
            } else {
                rhs = fmt::format("vec{}(0.0)", num_comp_count);
            }
        } else {
            rhs = "0";
        }
    }

    if (lhs.empty() || rhs.empty()) {
        LOG_ERROR("Source not loaded (lhs: {}, rhs: {})", lhs, rhs);
        return "";
    }

    if (num_comp_count > 1) {
        return fmt::format("{}({}, {})", compare_functions[compare_include_equal][sign_test], lhs, rhs);
    }

    return fmt::format("({}) {} {}", lhs, compare_ops[compare_include_equal][sign_test], rhs);
}

bool USSETranslatorVisitorGLSL::vtst(
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

    writer.add_to_current_body(fmt::format("p{} = {};", decoded_inst.opr.dest.num, vtst_impl(decoded_inst, pred, zero_test, sign_test, decoded_source_mask, false)));

    return true;
}

bool USSETranslatorVisitorGLSL::vtstmsk(
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

    std::string compare_result = vtst_impl(decoded_inst, pred, zero_test, sign_test, 0b1111, true);

    switch (decoded_inst.opr.dest.type) {
    case DataType::UINT8:
        compare_result = fmt::format("uvec4({}) * 255", compare_result);
        break;
    // TODO: If we support half4 then must convert to half4 instead!
    case DataType::F16:
    case DataType::F32:
        compare_result = fmt::format("vec4({})", compare_result);
        break;
    default:
        LOG_ERROR("Unexpected data type to convert to compatible type from VTSTMSK!");
        break;
    }

    // These can't be stored to Predicate registers obiviously
    variables.store(decoded_inst.opr.dest, compare_result, 0b1111, 0);
    return true;
}