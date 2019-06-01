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
    dest_mask = decode_write_mask(dest_mask, move_data_type == DataType::F16);

    // TODO: dest mask
    // TODO: flags
    // TODO: test type

    const bool is_double_regs = move_data_type == DataType::C10 || move_data_type == DataType::F16 || move_data_type == DataType::F32;
    const bool is_conditional = (move_type != MoveType::UNCONDITIONAL);

    // Decode operands
    uint8_t reg_bits = is_double_regs ? 7 : 6;

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, is_double_regs, reg_bits, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, is_double_regs, reg_bits, m_second_program);

    // Velocity uses a vec4 table, non-extended, so i assumes type=vec4, extended=false
    inst.opr.src1.swizzle = decode_vec34_swizzle(src0_swiz, false, 2);

    inst.opr.src1.type = move_data_type;
    inst.opr.dest.type = move_data_type;

    // TODO: adjust dest mask if needed

    if (inst.opr.dest.bank == RegisterBank::SPECIAL || inst.opr.src0.bank == RegisterBank::SPECIAL || inst.opr.src1.bank == RegisterBank::SPECIAL || inst.opr.src2.bank == RegisterBank::SPECIAL) {
        LOG_WARN("Special regs unsupported");
        return false;
    }

    spv::Id conditional_result = 0;
    CompareMethod compare_method = CompareMethod::NE_ZERO;
    spv::Op compare_op = spv::OpAny;
    
    if (is_conditional) {
        compare_method = static_cast<CompareMethod>((test_bit_2 << 1) | test_bit_1);
        inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank_sel, end_or_src0_bank_ext, is_double_regs, reg_bits, m_second_program);
        inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, is_double_regs, reg_bits, m_second_program);
        
        if (src0_comp_sel) {
            inst.opr.src0.swizzle = inst.opr.src1.swizzle;
        }

        inst.opr.src2.swizzle = inst.opr.src1.swizzle;

        const bool isUInt = is_unsigned_integer_data_type(inst.opr.src0.type);
        const bool isInt = is_signed_integer_data_type(inst.opr.src1.type);

        switch (compare_method) {
        case CompareMethod::LT_ZERO:
            if (isUInt)
                compare_op = spv::Op::OpULessThan;
            else if (isInt)
                compare_op = spv::Op::OpSLessThan;
            else
                compare_op = spv::Op::OpFOrdLessThan;
            break;
        case CompareMethod::LTE_ZERO:
            if (isUInt)
                compare_op = spv::Op::OpULessThanEqual;
            else if (isInt)
                compare_op = spv::Op::OpSLessThanEqual;
            else
                compare_op = spv::Op::OpFOrdLessThanEqual;
            break;
        case CompareMethod::NE_ZERO:
            if (isInt || isUInt)
                compare_op = spv::Op::OpINotEqual;
            else
                compare_op = spv::Op::OpFOrdNotEqual;
            break;
        case CompareMethod::EQ_ZERO:
            if (isInt || isUInt)
                compare_op = spv::Op::OpIEqual;
            else
                compare_op = spv::Op::OpFOrdEqual;
            break;
        }
    }

    // Recompile

    m_b.setLine(m_recompiler.cur_pc);

    spv::Function *link_sub = nullptr;
    
    if (is_conditional) {
        link_sub = m_recompiler.get_or_recompile_block(m_recompiler.avail_blocks[m_recompiler.cur_pc + 1]);
    }

    BEGIN_REPEAT(repeat_count, 2)
    GET_REPEAT(inst);

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
        conditional_str = fmt::format("({} {} {})", disasm::operand_to_str(inst.opr.src2, dest_mask), expr, disasm::operand_to_str(inst.opr.src0, dest_mask));
    }

    const std::string disasm_str = fmt::format("{:016x}: {}{}.{} {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode), disasm::data_type_str(move_data_type),
        disasm::operand_to_str(inst.opr.dest, dest_mask, dest_repeat_offset), disasm::operand_to_str(inst.opr.src1, dest_mask, src1_repeat_offset), conditional_str);

    LOG_DISASM(disasm_str);
    
    std::unique_ptr<spv::Builder::If> cond_builder;

    if (is_conditional) {
        spv::Id src0 = load(inst.opr.src0, dest_mask, src0_repeat_offset);
        spv::Id src2 = load(inst.opr.src2, dest_mask, src2_repeat_offset);

        conditional_result = m_b.createBinOp(compare_op, m_b.makeVectorType(m_b.makeBoolType(), m_b.getNumComponents(src0)),
            src2, src0);

        if (m_b.getNumComponents(conditional_result) > 1) {
            // We need to check if all bool is true, using OpAll
            conditional_result = m_b.createUnaryOp(spv::OpAll, m_b.makeBoolType(), conditional_result);
        }

        cond_builder = std::make_unique<spv::Builder::If>(conditional_result, spv::SelectionControlMaskNone, m_b);
    }
    
    spv::Id source = load(inst.opr.src1, dest_mask, src1_repeat_offset);
    store(inst.opr.dest, source, dest_mask, dest_repeat_offset);

    if (is_conditional) {
        cond_builder->makeEndIf();
        // Create call to next subroutine (block)
        m_b.createFunctionCall(link_sub, {});
    }
    
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::vpck(
    ExtPredicate pred,
    bool skipinv,
    bool nosched,
    Imm1 src2_bank_sel,
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
    Imm7 dest_n,
    Imm2 comp_sel_3,
    Imm1 scale,
    Imm2 comp_sel_1,
    Imm2 comp_sel_2,
    Imm5 src1_n,
    Imm1 comp0_sel_bit1,
    Imm4 src2_n,
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
    
    const spv::Op repack_opcode[][static_cast<int>(DataType::TOTAL_TYPE)] = {
        { spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpConvertUToF,
            spv::OpConvertUToF,
            spv::OpAll },
        { spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpConvertSToF,
            spv::OpConvertSToF,
            spv::OpAll },
        { spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll },
        { spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpConvertUToF,
            spv::OpConvertUToF,
            spv::OpAll },
        { spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpConvertUToF,
            spv::OpConvertUToF,
            spv::OpAll },
        { spv::OpConvertFToU,
            spv::OpConvertFToS,
            spv::OpAll,
            spv::OpConvertFToU,
            spv::OpConvertFToS,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll },
        { spv::OpConvertFToU,
            spv::OpConvertFToS,
            spv::OpAll,
            spv::OpConvertFToU,
            spv::OpConvertFToS,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll },
        { spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll,
            spv::OpAll }
    };

    inst.opcode = op_table[dest_fmt][src_fmt];

    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode));

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, false, 7, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, true, 7, m_second_program);

    inst.opr.dest.type = dest_data_type_table[dest_fmt];
    inst.opr.src1.type = src_data_type_table[src_fmt];

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

    disasm_str += fmt::format(" {} {}", disasm::operand_to_str(inst.opr.dest, dest_mask), disasm::operand_to_str(inst.opr.src1, dest_mask));
    LOG_DISASM(disasm_str);

    // Recompile
    m_b.setLine(m_recompiler.cur_pc);

    BEGIN_REPEAT(repeat_count, 2)
    GET_REPEAT(inst);

    spv::Id source = load(inst.opr.src1, dest_mask, src1_repeat_offset);

    if (repack_opcode[dest_fmt][src_fmt] != spv::OpAll) {
        // Do conversion
        spv::Id dest_type = type_f32;

        if (is_signed_integer_data_type(inst.opr.dest.type)) {
            dest_type = m_b.makeIntType(32);
        } else if (is_unsigned_integer_data_type(inst.opr.dest.type)) {
            dest_type = type_ui32;
        }

        std::vector<spv::Id> ops { source };
        source = m_b.createOp(repack_opcode[dest_fmt][src_fmt], m_b.makeVectorType(dest_type, m_b.getNumComponents(source)),
            ops);
    }
    
    store(inst.opr.dest, source, dest_mask, dest_repeat_offset);
    END_REPEAT()

    return true;
}
