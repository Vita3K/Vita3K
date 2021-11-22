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

#include <shader/usse_decoder_helpers.h>
#include <shader/usse_disasm.h>
#include <shader/usse_translator.h>
#include <shader/usse_types.h>
#include <util/log.h>

using namespace shader;
using namespace usse;

static Opcode decode_test_inst(const Imm2 alu_sel, const Imm4 alu_op, const Imm1 prec, DataType &filled_dt) {
    static const Opcode vf16_opcode[16] = {
        Opcode::INVALID,
        Opcode::INVALID,
        Opcode::VF16ADD,
        Opcode::VF16FRC,
        Opcode::VRCP,
        Opcode::VRSQ,
        Opcode::VLOG,
        Opcode::VEXP,
        Opcode::VF16DP,
        Opcode::VF16MIN,
        Opcode::VF16MAX,
        Opcode::VF16DSX,
        Opcode::VF16DSY,
        Opcode::VF16MUL,
        Opcode::VF16SUB,
        Opcode::INVALID,
    };

    static const Opcode vf32_opcode[16] = {
        Opcode::INVALID,
        Opcode::INVALID,
        Opcode::VADD,
        Opcode::VFRC,
        Opcode::VRCP,
        Opcode::VRSQ,
        Opcode::VLOG,
        Opcode::VEXP,
        Opcode::VDP,
        Opcode::VMIN,
        Opcode::VMAX,
        Opcode::VDSX,
        Opcode::VDSY,
        Opcode::VMUL,
        Opcode::VSUB,
        Opcode::INVALID,
    };

    static const Opcode test_ops[4][16] = {
        {
            Opcode::FADD,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::FSUBFLR,
            Opcode::FRCP,
            Opcode::FRSQ,
            Opcode::FLOG,
            Opcode::FEXP,
            Opcode::FDP,
            Opcode::FMIN,
            Opcode::FMAX,
            Opcode::FDSX,
            Opcode::FDSY,
            Opcode::FMUL,
            Opcode::FSUB,
            Opcode::INVALID,
        },
        { Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::IADD16,
            Opcode::ISUB16,
            Opcode::IMUL16,
            Opcode::IADDU16,
            Opcode::ISUBU16,
            Opcode::IMULU16,
            Opcode::IADD32,
            Opcode::IADDU32,
            Opcode::ISUB32,
            Opcode::ISUBU32 },
        { Opcode::IADD8,
            Opcode::ISUB8,
            Opcode::IADDU8,
            Opcode::ISUBU8,
            Opcode::IMUL8,
            Opcode::FPMUL8,
            Opcode::IMULU8,
            Opcode::FPADD8,
            Opcode::FPSUB8,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID },
        { Opcode::AND,
            Opcode::OR,
            Opcode::XOR,
            Opcode::SHL,
            Opcode::SHR,
            Opcode::ROL,
            Opcode::INVALID,
            Opcode::ASR,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID,
            Opcode::INVALID }
    };

    if (alu_sel == 0) {
        switch (prec) {
        case 0: {
            filled_dt = DataType::F16;
            return vf16_opcode[alu_op];
        }

        case 1: {
            filled_dt = DataType::F32;
            return vf32_opcode[alu_op];
        }

        default: return Opcode::INVALID;
        }
    }

    const auto test_op = test_ops[alu_sel][alu_op];

    switch (test_op) {
    case Opcode::IADDU32:
    case Opcode::ISUBU32: {
        filled_dt = DataType::UINT32;
        break;
    }
    case Opcode::ISUB32:
    case Opcode::IADD32: {
        filled_dt = DataType::INT32;
        break;
    }
    case Opcode::IADD16:
    case Opcode::ISUB16:
    case Opcode::IMUL16: {
        filled_dt = DataType::INT16;
        break;
    }
    case Opcode::IADDU16:
    case Opcode::ISUBU16:
    case Opcode::IMULU16: {
        filled_dt = DataType::UINT16;
        break;
    }
    case Opcode::IADD8:
    case Opcode::ISUB8:
    case Opcode::IMUL8: {
        filled_dt = DataType::INT8;
        break;
    }
    case Opcode::IADDU8:
    case Opcode::ISUBU8:
    case Opcode::FPSUB8:
    case Opcode::IMULU8: {
        filled_dt = DataType::UINT8;
        break;
    }
    }

    if (alu_sel == 2) {
        filled_dt = DataType::UINT8;
    }

    if (alu_sel == 3) {
        filled_dt = DataType::UINT32;
    }

    return test_op;
}

inline bool is_sub_opcode(Opcode test_op) {
    return (test_op == Opcode::VSUB) || (test_op == Opcode::VF16SUB) || (test_op == Opcode::ISUB8) || (test_op == Opcode::ISUB16) || (test_op == Opcode::ISUB32) || (test_op == Opcode::ISUBU8) || (test_op == Opcode::ISUBU16) || (test_op == Opcode::ISUBU32) || (test_op == Opcode::FPSUB8);
}

spv::Id USSETranslatorVisitor::vtst_impl(Instruction inst, ExtPredicate pred, int zero_test, int sign_test, Imm4 load_mask, bool mask) {
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
        { { spv::OpIEqual,
              spv::OpSLessThan,
              spv::OpSGreaterThan,
              spv::OpAll },
            { spv::OpIEqual,
                spv::OpSLessThanEqual,
                spv::OpSGreaterThanEqual,
                spv::OpAll } },
        { { spv::OpIEqual,
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

        lhs = do_alu_op(inst, load_mask);

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

bool USSETranslatorVisitor::vtst(
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
    DataType load_data_type;
    Opcode test_op = decode_test_inst(alu_sel, alu_op, prec, load_data_type);

    if (test_op == Opcode::INVALID) {
        LOG_ERROR("Unsupported comparision: {} {}", alu_sel, alu_op);
        return false;
    }

    Instruction inst;
    inst.opcode = test_op;

    const Imm4 tb_decode_load_mask[] = {
        0b0001,
        0b0010,
        0b0100,
        0b1000,
        0b0000,
        0b0000,
        0b0000
    };

    if (chan_cc >= 4) {
        LOG_ERROR("Unsupported comparision with requested channels ordinal: {}", chan_cc);
        return false;
    }

    const Imm4 load_mask = tb_decode_load_mask[chan_cc];

    const bool use_double_reg = alu_sel == 0;
    const uint8_t bits_max = use_double_reg ? 8 : 7;

    // Build up source
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_ext, use_double_reg, bits_max, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_ext, use_double_reg, bits_max, m_second_program);

    inst.opr.src1.type = load_data_type;
    inst.opr.src2.type = load_data_type;

    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.src2.swizzle = (src2_vscomp && (alu_sel == 0)) ? (Swizzle4 SWIZZLE_CHANNEL_4(X, X, X, X)) : (Swizzle4 SWIZZLE_CHANNEL_4_DEFAULT);

    if (src1_neg) {
        inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    Operand pred_op{};
    pred_op.bank = RegisterBank::PREDICATE;
    pred_op.num = pdst_n;
    inst.opr.dest = pred_op;

    const spv::Id pred_result = vtst_impl(inst, pred, zero_test, sign_test, load_mask, false);

    store(inst.opr.dest, pred_result);
    return true;
}

bool USSETranslatorVisitor::vtstmsk(
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
    DataType load_data_type;
    Opcode test_op = decode_test_inst(alu_sel, alu_op, prec, load_data_type);
    Imm4 dest_mask;
    Instruction inst;

    if (test_op == Opcode::INVALID) {
        LOG_ERROR("Unsupported comparision: {} {}", alu_sel, alu_op);
        return false;
    }

    inst.opcode = test_op;

    if (tst_mask_type == 3) {
        LOG_ERROR("Invalid test mask type: 3");
        return false;
    }

    DataType store_data_type;
    switch (tst_mask_type) {
    case 0:
        store_data_type = DataType::UINT8;
        break;
    case 1:
        store_data_type = DataType::F16;
        break;
    case 2:
        store_data_type = DataType::F32;
        break;
    default:
        LOG_ERROR("Invalid test mask type: {}", tst_mask_type);
        return false;
    }

    const bool use_double_reg = alu_sel == 0;
    // TODO: In some op, we don't need to load src2 e.g. rsq
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_ext, use_double_reg, 8, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_ext, use_double_reg, 8, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_ext, use_double_reg, 8, m_second_program);

    inst.opr.src1.type = load_data_type;
    inst.opr.src2.type = load_data_type;
    inst.opr.dest.type = store_data_type;

    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.src2.swizzle = src2_vscomp ? (Swizzle4 SWIZZLE_CHANNEL_4(X, X, X, X)) : (Swizzle4 SWIZZLE_CHANNEL_4_DEFAULT);
    inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    if (test_flag_2) {
        inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    if (!test_wben) {
        // TODO support it
        LOG_ERROR("Write disabled vmsktst is not supported yet.");
        return false;
    }

    spv::Id pred_result = vtst_impl(inst, pred, zero_test, sign_test, 0b1111, true);

    spv::Id float_v4 = utils::make_vector_or_scalar_type(m_b, m_b.makeFloatType(32), 4);
    spv::Id uint_v4 = utils::make_vector_or_scalar_type(m_b, m_b.makeUintType(32), 4);
    spv::Id scaler;
    switch (store_data_type) {
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

    store(inst.opr.dest, pred_result);

    return true;
}

bool USSETranslatorVisitor::br(
    ExtPredicate pred,
    Imm1 syncend,
    bool exception,
    bool pwait,
    Imm1 sync_ext,
    bool nosched,
    bool br_monitor,
    bool save_link,
    Imm1 br_type,
    Imm1 any_inst,
    Imm1 all_inst,
    uint32_t br_off) {
    assert(false && "Unreachable");

    LOG_ERROR("Branch instruction should not be recompiled here!");
    return true;
}
