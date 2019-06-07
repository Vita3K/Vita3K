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
            Opcode::INVALID,
            Opcode::INVALID },
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

    if (alu_sel == 3) {
        filled_dt = DataType::UINT32;
    }

    return test_ops[alu_sel][alu_op];
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
    // Usually we would expect this to have a compare behavior
    // Comparision is done by subtracting the first src by the second src, and compare the result value.
    // We currently optimize for that case first
    DataType load_data_type;
    Opcode test_op = decode_test_inst(alu_sel, alu_op, prec, load_data_type);

    if (test_op == Opcode::INVALID) {
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

    // Build up source
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_ext, true, 8, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_ext, true, 8, m_second_program);

    inst.opr.src1.type = load_data_type;
    inst.opr.src2.type = load_data_type;

    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.src2.swizzle = src2_vscomp ? (Swizzle4 SWIZZLE_CHANNEL_4(X, X, X, X)) : (Swizzle4 SWIZZLE_CHANNEL_4_DEFAULT);

    if (src1_neg) {
        inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    // Load our compares
    spv::Id lhs = spv::NoResult;
    spv::Id rhs = spv::NoResult;

    spv::Id pred_result = m_b.makeBoolConstant(true);

    // Zero test number:
    // 0 - alway pass
    // 1 - zero
    // 2 - non-zero
    const bool compare_include_equal = (zero_test == 1);

    // Sign test number
    const spv::Op tb_comp_ops[2][4] = {
        { spv::OpFOrdNotEqual,
            spv::OpFOrdLessThan,
            spv::OpFOrdGreaterThan,
            spv::OpAll },
        { spv::OpFOrdEqual,
            spv::OpFOrdLessThanEqual,
            spv::OpFOrdGreaterThanEqual,
            spv::OpAll }
    };

    const spv::Op used_comp_op = tb_comp_ops[compare_include_equal][sign_test];

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

    if (test_op == Opcode::VSUB || test_op == Opcode::VF16SUB) {
        LOG_DISASM("{:016x}: {}{}.{}.{} p{} {} {}", m_instr, disasm::e_predicate_str(pred), "CMP", used_comp_str, disasm::data_type_str(load_data_type),
            pdst_n, disasm::operand_to_str(inst.opr.src1, load_mask), disasm::operand_to_str(inst.opr.src2, load_mask));

        lhs = load(inst.opr.src1, load_mask);
        rhs = load(inst.opr.src2, load_mask);
    } else {
        LOG_DISASM("{:016x}: {}{}.{}zero.{} p{} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(test_op), used_comp_str, disasm::data_type_str(load_data_type),
            pdst_n, disasm::operand_to_str(inst.opr.src1, load_mask), disasm::operand_to_str(inst.opr.src2, load_mask));

        lhs = do_alu_op(inst, load_mask);

        spv::Id c0 = m_b.makeFloatConstant(0.0f);

        if (is_signed_integer_data_type(load_data_type)) {
            c0 = m_b.makeIntConstant(0);
        } else if (is_unsigned_integer_data_type(load_data_type)) {
            c0 = m_b.makeUintConstant(0);
        }

        std::vector<spv::Id> comps(m_b.getNumComponents(c0), c0);

        if (comps.size() > 1) {
            rhs = m_b.makeCompositeConstant(m_b.getTypeId(c0), comps);
        } else {
            rhs = c0;
        }
    }

    if (lhs == spv::NoResult || rhs == spv::NoResult) {
        LOG_ERROR("Source not loaded (lhs: {}, rhs: {})", lhs, rhs);
        return false;
    }

    pred_result = m_b.createOp(used_comp_op, m_b.makeBoolType(), { lhs, rhs });

    Operand pred_op{};
    pred_op.bank = RegisterBank::PREDICATE;
    pred_op.num = pdst_n;

    store(pred_op, pred_result);
    return true;
}

bool USSETranslatorVisitor::vtstmsk(
    Imm3 pred,
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
    LOG_ERROR("VTSTMASK unimplemented!");
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
    std::uint32_t br_off) {
    Opcode op = (br_type == 0) ? Opcode::BA : Opcode::BR;

    if (op == Opcode::BR && (br_off & (1 << 19))) {
        // PC bits on SGX543 is 20 bits
        br_off |= 0xFFFFFFFF << 20;
    }

    auto cur_pc = m_recompiler.cur_pc;

    LOG_DISASM("{:016x}: {}{} #{}", m_instr, disasm::e_predicate_str(pred), (br_type == 0) ? "BA" : "BR", br_off + cur_pc);
    spv::Function *br_block = m_recompiler.get_or_recompile_block(m_recompiler.avail_blocks[br_off + cur_pc]);

    m_b.setLine(m_recompiler.cur_pc);

    if (pred == ExtPredicate::NONE) {
        m_b.createFunctionCall(br_block, {});
    } else {
        spv::Id pred_v = spv::NoResult;

        Operand pred_opr{};
        pred_opr.bank = RegisterBank::PREDICATE;

        bool do_neg = false;

        if (pred >= ExtPredicate::P0 && pred <= ExtPredicate::P3) {
            pred_opr.num = static_cast<int>(pred) - static_cast<int>(ExtPredicate::P0);
        } else if (pred >= ExtPredicate::NEGP0 && pred <= ExtPredicate::NEGP1) {
            pred_opr.num = static_cast<int>(pred) - static_cast<int>(ExtPredicate::NEGP0);
            do_neg = true;
        }

        pred_v = load(pred_opr, 0b0001);

        if (pred_v == spv::NoResult) {
            LOG_ERROR("Pred not loaded");
            return false;
        }

        if (do_neg) {
            std::vector<spv::Id> ops{ pred_v };
            pred_v = m_b.createOp(spv::OpLogicalNot, m_b.makeBoolType(), ops);
        }

        spv::Function *continous_block = m_recompiler.get_or_recompile_block(m_recompiler.avail_blocks[cur_pc + 1]);
        spv::Builder::If cond_builder(pred_v, spv::SelectionControlMaskNone, m_b);

        m_b.createFunctionCall(br_block, {});
        cond_builder.makeBeginElse();
        m_b.createFunctionCall(continous_block, {});
        cond_builder.makeEndIf();
    }

    return true;
}
