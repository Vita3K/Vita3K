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

    Imm8 src1_mask = dest_mask;
    Imm8 src2_mask = 0;

    constexpr Imm4 SRC1_LOAD_MASKS[] = { 0, 0b1, 0b11 };
    constexpr Imm4 SRC2_LOAD_MASKS[] = { 0, 0b10, 0b1100 };

    if (inst.opr.src1.type == DataType::F32) {
        // Need another op
        inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, true, 7, m_second_program);
        
        const int src1_comp_handle = (static_cast<int>(dest_mask_to_comp_count(dest_mask) + 1) >> 1);
        const int src2_comp_handle = static_cast<int>(dest_mask_to_comp_count(dest_mask)) - src1_comp_handle;

        const bool is_two_source_bank_different = inst.opr.src1.bank != inst.opr.src2.bank;
        const bool src2_needed = src2_comp_handle != 0;
        const bool is_two_source_contiguous = (inst.opr.src1.num + src1_comp_handle == inst.opr.src2.num) && 
            !is_two_source_bank_different;

        int offset_start_masking = 0;

        for (int i = 0; i < 4; i++) {
            if (dest_mask & (1 << i)) {
                offset_start_masking = i;
                break;
            }
        }

        // We need to explicitly load src2 when we are fall in these situations:
        // - First, src2 must be needed. The only situation we knows it isn't needed is when the dest mask comp count is 1.
        //   In that case, only src1 is used.
        // - Two sources are not contiguous. We call it contigous when two sources are not in the same bank. Also,
        //   the total component that src1 handles, when adding with src1 offset, must reached exactly to offset where src2
        //   resides
        //
        // Case where this doesn't apply (found when testing, need more confirmation)
        // - src1 bank is FPINTERNAL.
        if (!is_two_source_contiguous && src2_needed && inst.opr.src1.bank != RegisterBank::FPINTERNAL) {
            // Seperate the mask
            src1_mask = ((dest_mask >> offset_start_masking) & SRC1_LOAD_MASKS[src1_comp_handle]) << offset_start_masking;
            src2_mask = (((dest_mask >> offset_start_masking) & SRC2_LOAD_MASKS[src2_comp_handle]) >> src1_comp_handle) << offset_start_masking;

            // Performs swizzle cycling
            // Calculate the offset distance from src2 to src1.
            const Imm6 diff_off = inst.opr.src2.num - inst.opr.src1.num;
            const Imm6 src2_swizz_start = (diff_off % 4);

            for (Imm6 src2_swizz_indx = src2_swizz_start; src2_swizz_indx < 4; src2_swizz_indx++) {
                // src2_swizz_indx is the index of src2 swizzle in src1 swizzle
                inst.opr.src2.swizzle[src2_swizz_indx - src2_swizz_start] = inst.opr.src1.swizzle[src2_swizz_indx];
            }
        }
    }

    // Recompile
    m_b.setLine(m_recompiler.cur_pc);

    BEGIN_REPEAT(repeat_count, 2)
    GET_REPEAT(inst);

    if (src2_mask != 0) {
        LOG_DISASM("{} {} ({} {})", disasm_str, disasm::operand_to_str(inst.opr.dest, dest_mask, dest_repeat_offset),
            disasm::operand_to_str(inst.opr.src1, src1_mask, src1_repeat_offset),
            disasm::operand_to_str(inst.opr.src2, src2_mask, src2_repeat_offset));
    } else {
         LOG_DISASM("{} {} {}", disasm_str, disasm::operand_to_str(inst.opr.dest, dest_mask, dest_repeat_offset),
            disasm::operand_to_str(inst.opr.src1, dest_mask, src1_repeat_offset));
    }

    spv::Id source = load(inst.opr.src1, src1_mask, src1_repeat_offset);

    if (src2_mask != 0) {
        // Need to load this also, then create a merge between these twos
        spv::Id source2 = load(inst.opr.src2, src2_mask, src2_repeat_offset);

        // Merge using op vector shuffle
        const int num_comp = static_cast<int>(dest_mask_to_comp_count(dest_mask));

        spv::Id vector_type = m_b.getTypeId(source2);

        if (!m_b.isScalarType(vector_type)) {
            vector_type = m_b.getContainedTypeId(vector_type);
        }

        vector_type = m_b.makeVectorType(vector_type, num_comp);

        if (num_comp == 2) {
            // Each source currently only contains 1 component. Create a composite construct
            source = m_b.createCompositeConstruct(vector_type, { source, source2 });
        } else {
            std::vector<spv::Id> merge_ops { source, source2 };
            merge_ops.resize(2 + num_comp);

            std::iota(merge_ops.begin() + 2, merge_ops.end(), 0);
            source = m_b.createOp(spv::OpVectorShuffle, vector_type, merge_ops);
        }
    }

    if (repack_opcode[dest_fmt][src_fmt] != spv::OpAll) {
        // Do conversion
        spv::Id dest_type = type_f32;

        if (is_signed_integer_data_type(inst.opr.dest.type)) {
            dest_type = m_b.makeIntType(32);
        } else if (is_unsigned_integer_data_type(inst.opr.dest.type)) {
            dest_type = type_ui32;
        }

        std::vector<spv::Id> ops { source };
        source = m_b.createOp(repack_opcode[dest_fmt][src_fmt], m_b.makeVectorType(dest_type,
            m_b.getNumComponents(source)), ops);
    }
    
    store(inst.opr.dest, source, dest_mask, dest_repeat_offset);
    END_REPEAT()

    return true;
}
