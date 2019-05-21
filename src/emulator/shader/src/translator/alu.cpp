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

bool USSETranslatorVisitor::vmad(
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 gpi1_swiz_ext,
    Imm1 opcode2,
    Imm1 dest_use_bank_ext,
    Imm1 end,
    Imm1 src0_bank_ext,
    Imm2 increment_mode,
    Imm1 gpi0_abs,
    RepeatCount repeat_count,
    bool nosched,
    Imm4 write_mask,
    Imm1 src0_neg,
    Imm1 src0_abs,
    Imm1 gpi1_neg,
    Imm1 gpi1_abs,
    Imm1 gpi0_swiz_ext,
    Imm2 dest_bank,
    Imm2 src0_bank,
    Imm2 gpi0_n,
    Imm6 dest_n,
    Imm4 gpi0_swiz,
    Imm4 gpi1_swiz,
    Imm2 gpi1_n,
    Imm1 gpi0_neg,
    Imm1 src0_swiz_ext,
    Imm4 src0_swiz,
    Imm6 src0_n) {
    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(pred), "VMAD");

    Instruction inst{};

    // Is this VMAD3 or VMAD4, op2 = 0 => vec3
    int type = 2;

    if (opcode2 == 0) {
        type = 1;
    }

    // Double regs always true for src0, dest
    inst.opr.src0 = decode_src12(inst.opr.src0, src0_n, src0_bank, src0_bank_ext, true, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_use_bank_ext, true, 7, m_second_program);

    // GPI0 and GPI1, setup!
    inst.opr.src1.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src1.num = gpi0_n;

    inst.opr.src2.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src2.num = gpi1_n;

    // Swizzleee
    if (type == 1) {
        inst.opr.dest.swizzle[3] = usse::SwizzleChannel::_X;
    }

    inst.opr.src0.swizzle = decode_vec34_swizzle(src0_swiz, src0_swiz_ext, type);
    inst.opr.src1.swizzle = decode_vec34_swizzle(gpi0_swiz, gpi0_swiz_ext, type);
    inst.opr.src2.swizzle = decode_vec34_swizzle(gpi1_swiz, gpi1_swiz_ext, type);

    if (src0_abs) {
        inst.opr.src0.flags |= RegisterFlags::Absolute;
    }

    if (src0_neg) {
        inst.opr.src0.flags |= RegisterFlags::Negative;
    }

    if (gpi0_abs) {
        inst.opr.src1.flags |= RegisterFlags::Absolute;
    }

    if (gpi0_neg) {
        inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    if (gpi1_abs) {
        inst.opr.src2.flags |= RegisterFlags::Absolute;
    }

    if (gpi1_neg) {
        inst.opr.src2.flags |= RegisterFlags::Negative;
    }

    disasm_str += fmt::format(" {} {} {} {}", disasm::operand_to_str(inst.opr.dest, write_mask), disasm::operand_to_str(inst.opr.src0, write_mask), disasm::operand_to_str(inst.opr.src1, write_mask), disasm::operand_to_str(inst.opr.src2, write_mask));

    LOG_DISASM("{}", disasm_str);

    m_b.setLine(m_recompiler.cur_pc);

    // Write mask is a 4-bit immidiate
    // If a bit is one, a swizzle is active
    BEGIN_REPEAT(repeat_count, 2)
    GET_REPEAT(inst);

    spv::Id vsrc0 = load(inst.opr.src0, write_mask, src0_repeat_offset);
    spv::Id vsrc1 = load(inst.opr.src1, write_mask, src1_repeat_offset);
    spv::Id vsrc2 = load(inst.opr.src2, write_mask, src2_repeat_offset);

    if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        return false;
    }

    auto mul_result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    store(inst.opr.dest, add_result, write_mask, dest_repeat_offset);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::vmad2(
    Imm1 dat_fmt,
    Imm2 pred,
    Imm1 skipinv,
    Imm1 src0_swiz_bits2,
    Imm1 syncstart,
    Imm1 src0_abs,
    Imm3 src2_swiz,
    Imm1 src1_swiz_bit2,
    Imm1 nosched,
    Imm4 dest_mask,
    Imm2 src1_mod,
    Imm2 src2_mod,
    Imm1 src0_bank,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm6 dest_n,
    Imm2 src1_swiz_bits01,
    Imm2 src0_swiz_bits01,
    Imm6 src0_n,
    Imm6 src1_n,
    Imm6 src2_n) {
    Instruction inst{};
    Opcode op = Opcode::VMAD;

    // If dat_fmt equals to 1, the type of instruction is F16
    if (dat_fmt) {
        op = Opcode::VF16MAD;
    }

    const DataType inst_dt = (dat_fmt) ? DataType::F16 : DataType::F32;

    // Decode destination mask
    dest_mask = decode_write_mask(dest_mask, dat_fmt);

    // Decode mandantory info first
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, false, true, 7, m_second_program);
    inst.opr.src0 = decode_src0(inst.opr.src0, src0_n, src0_bank, false, true, 7, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, false, true, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, false, true, 7, m_second_program);

    // Fill in data type
    inst.opr.dest.type = inst_dt;
    inst.opr.src0.type = inst_dt;
    inst.opr.src1.type = inst_dt;
    inst.opr.src2.type = inst_dt;

    // Decode swizzle
    const Swizzle4 tb_decode_src0_swiz[] = {
        SWIZZLE_CHANNEL_4(X, X, X, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, Y),
        SWIZZLE_CHANNEL_4(Z, Z, Z, Z),
        SWIZZLE_CHANNEL_4(W, W, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, W),
        SWIZZLE_CHANNEL_4(Y, Z, X, W),
        SWIZZLE_CHANNEL_4(X, Y, W, W),
        SWIZZLE_CHANNEL_4(Z, W, X, Y)
    };

    const Swizzle4 tb_decode_src1_swiz[] = {
        SWIZZLE_CHANNEL_4(X, X, X, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, Y),
        SWIZZLE_CHANNEL_4(Z, Z, Z, Z),
        SWIZZLE_CHANNEL_4(W, W, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, W),
        SWIZZLE_CHANNEL_4(X, Y, Y, Z),
        SWIZZLE_CHANNEL_4(Y, Y, W, W),
        SWIZZLE_CHANNEL_4(W, Y, Z, W)
    };

    const Swizzle4 tb_decode_src2_swiz[] = {
        SWIZZLE_CHANNEL_4(X, X, X, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, Y),
        SWIZZLE_CHANNEL_4(Z, Z, Z, Z),
        SWIZZLE_CHANNEL_4(W, W, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, W),
        SWIZZLE_CHANNEL_4(X, Z, W, W),
        SWIZZLE_CHANNEL_4(X, X, Y, Z),
        SWIZZLE_CHANNEL_4(X, Y, Z, Z)
    };

    inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.src0.swizzle = tb_decode_src0_swiz[(src0_swiz_bits01 | src0_swiz_bits2 << 1)];
    inst.opr.src1.swizzle = tb_decode_src1_swiz[(src1_swiz_bits01 | src1_swiz_bit2 << 1)];
    inst.opr.src2.swizzle = tb_decode_src2_swiz[src2_swiz];

    // Decode modifiers
    if (src0_abs) {
        inst.opr.src0.flags |= RegisterFlags::Absolute;
    }

    inst.opr.src1.flags = decode_modifier(src1_mod);
    inst.opr.src2.flags = decode_modifier(src1_mod);

    // Log the instruction
    LOG_DISASM("{:016x}: {}{}2 {} {} {} {}", m_instr, disasm::e_predicate_str(static_cast<ExtPredicate>(pred)), disasm::opcode_str(op), disasm::operand_to_str(inst.opr.dest, dest_mask),
        disasm::operand_to_str(inst.opr.src0, dest_mask), disasm::operand_to_str(inst.opr.src1, dest_mask), disasm::operand_to_str(inst.opr.src2, dest_mask));

    m_b.setLine(m_recompiler.cur_pc);

    // Translate the instruction
    spv::Id vsrc0 = load(inst.opr.src0, dest_mask, 0);
    spv::Id vsrc1 = load(inst.opr.src1, dest_mask, 0);
    spv::Id vsrc2 = load(inst.opr.src2, dest_mask, 0);

    if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        return false;
    }

    auto mul_result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    store(inst.opr.dest, add_result, dest_mask, 0);

    return true;
}

bool USSETranslatorVisitor::vdp(
    ExtPredicate pred,
    Imm1 skipinv,
    bool clip_plane_enable,
    Imm1 opcode2,
    Imm1 dest_use_bank_ext,
    Imm1 end,
    Imm1 src0_bank_ext,
    Imm2 increment_mode,
    Imm1 gpi0_abs,
    RepeatCount repeat_count,
    bool nosched,
    Imm4 write_mask,
    Imm1 src0_neg,
    Imm1 src0_abs,
    Imm3 clip_plane_n,
    Imm2 dest_bank,
    Imm2 src0_bank,
    Imm2 gpi0_n,
    Imm6 dest_n,
    Imm4 gpi0_swiz,
    Imm3 src0_swiz_w,
    Imm3 src0_swiz_z,
    Imm3 src0_swiz_y,
    Imm3 src0_swiz_x,
    Imm6 src0_n) {
    Instruction inst;

    // Is this VDP3 or VDP4, op2 = 0 => vec3
    int type = 2;
    usse::Imm4 src_mask = 0b1111;

    if (opcode2 == 0) {
        type = 1;
        src_mask = 0b0111;
    }

    // Double regs always true for src0, dest
    inst.opr.src0 = decode_src12(inst.opr.src0, src0_n, src0_bank, src0_bank_ext, true, 7, m_second_program);
    inst.opr.src0.index = 0;

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_use_bank_ext, true, 7, m_second_program);

    inst.opr.src1.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src1.num = gpi0_n;
    inst.opr.src1.index = 1;

    inst.opr.src1.swizzle = decode_vec34_swizzle(gpi0_swiz, false, type);

    // Decode first source swizzle
    const SwizzleChannel tb_swizz_dec[] = {
        SwizzleChannel::_X,
        SwizzleChannel::_Y,
        SwizzleChannel::_Z,
        SwizzleChannel::_W,
        SwizzleChannel::_0,
        SwizzleChannel::_1,
        SwizzleChannel::_2,
        SwizzleChannel::_H,
        SwizzleChannel::_UNDEFINED
    };

    inst.opr.src0.swizzle[0] = tb_swizz_dec[src0_swiz_x];
    inst.opr.src0.swizzle[1] = tb_swizz_dec[src0_swiz_y];
    inst.opr.src0.swizzle[2] = tb_swizz_dec[src0_swiz_z];
    inst.opr.src0.swizzle[3] = tb_swizz_dec[src0_swiz_w];

    // Set modifiers
    if (src0_neg) {
        inst.opr.src0.flags |= RegisterFlags::Negative;
    }

    if (src0_abs) {
        inst.opr.src0.flags |= RegisterFlags::Absolute;
    }

    if (gpi0_abs) {
        inst.opr.src1.flags |= RegisterFlags::Absolute;
    }

    m_b.setLine(m_recompiler.cur_pc);

    // Decoding done
    BEGIN_REPEAT(repeat_count, 2)
    GET_REPEAT(inst);

    LOG_DISASM("{:016x}: {}VDP {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::operand_to_str(inst.opr.dest, write_mask, dest_repeat_offset),
        disasm::operand_to_str(inst.opr.src0, src_mask, src0_repeat_offset), disasm::operand_to_str(inst.opr.src1, src_mask, src1_repeat_offset));

    spv::Id lhs = load(inst.opr.src0, type == 1 ? 0b0111 : 0b1111, src0_repeat_offset);
    spv::Id rhs = load(inst.opr.src1, type == 1 ? 0b0111 : 0b1111, src1_repeat_offset);

    spv::Id result = m_b.createBinOp(spv::OpDot, type_f32, lhs, rhs);

    store(inst.opr.dest, result, write_mask, dest_repeat_offset);
    END_REPEAT()

    return true;
}

spv::Id USSETranslatorVisitor::do_alu_op(Instruction &inst, const Imm4 dest_mask) {
    spv::Id vsrc1 = load(inst.opr.src1, dest_mask, 0);
    spv::Id vsrc2 = load(inst.opr.src2, dest_mask, 0);
    std::vector<spv::Id> ids;
    ids.push_back(vsrc1);

    if (vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        LOG_WARN("Could not find a src register");
        return spv::NoResult;
    }

    spv::Id result = spv::NoResult;
    spv::Id source_type = m_b.getTypeId(vsrc1);

    switch (inst.opcode) {
    case Opcode::VDSX:
    case Opcode::VF16DSX:
        result = m_b.createOp(spv::OpDPdx, source_type, ids);
        break;

    case Opcode::VDSY:
    case Opcode::VF16DSY:
        result = m_b.createOp(spv::OpDPdy, source_type, ids);
        break;

    case Opcode::VADD:
    case Opcode::VF16ADD: {
        result = m_b.createBinOp(spv::OpFAdd, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::VMUL:
    case Opcode::VF16MUL: {
        result = m_b.createBinOp(spv::OpFMul, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::VMIN:
    case Opcode::VF16MIN: {
        result = m_b.createBuiltinCall(source_type, std_builtins, GLSLstd450FMin, { vsrc1, vsrc2 });
        break;
    }

    case Opcode::VMAX:
    case Opcode::VF16MAX: {
        result = m_b.createBuiltinCall(source_type, std_builtins, GLSLstd450FMax, { vsrc1, vsrc2 });
        break;
    }

    case Opcode::VFRC:
    case Opcode::VF16FRC: {
        // Dest = Source1 - Floor(Source2)
        // If two source are identical, let's use the fractional function
        if (inst.opr.src1.is_same(inst.opr.src2, dest_mask)) {
            result = m_b.createBuiltinCall(source_type, std_builtins, GLSLstd450Fract, { vsrc1 });
        } else {
            // We need to floor source 2
            spv::Id source2_floored = m_b.createBuiltinCall(source_type, std_builtins, GLSLstd450Floor, { vsrc2 });
            // Then subtract source 1 with the floored source 2. TADA!
            result = m_b.createBinOp(spv::OpFSub, source_type, vsrc1, source2_floored);
        }

        break;
    }

    default: {
        LOG_ERROR("Unimplemented ALU opcode instruction: {}", disasm::opcode_str(inst.opcode));
        break;
    }
    }

    return result;
}

bool USSETranslatorVisitor::vnmad32(
    ExtPredicate pred,
    bool skipinv,
    Imm2 src1_swiz_10_11,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 src1_swiz_9,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm4 src2_swiz,
    bool nosched,
    Imm4 dest_mask,
    Imm2 src1_mod,
    Imm1 src2_mod,
    Imm2 src1_swiz_7_8,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm2 src2_bank_sel,
    Imm6 dest_n,
    Imm7 src1_swiz_0_6,
    Imm3 op2,
    Imm6 src1_n,
    Imm6 src2_n) {
    Instruction inst{};

    static const Opcode tb_decode_vop_f32[] = {
        Opcode::VMUL,
        Opcode::VADD,
        Opcode::VFRC,
        Opcode::VDSX,
        Opcode::VDSY,
        Opcode::VMIN,
        Opcode::VMAX,
        Opcode::VDP,
    };
    static const Opcode tb_decode_vop_f16[] = {
        Opcode::VF16MUL,
        Opcode::VF16ADD,
        Opcode::VF16FRC,
        Opcode::VF16DSX,
        Opcode::VF16DSY,
        Opcode::VF16MIN,
        Opcode::VF16MAX,
        Opcode::VF16DP,
    };
    Opcode opcode;
    const bool is_32_bit = m_instr & (1ull << 59);
    if (is_32_bit)
        opcode = tb_decode_vop_f32[op2];
    else
        opcode = tb_decode_vop_f16[op2];

    // Decode operands
    // TODO: modifiers

    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, true, 7, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, true, 7, m_second_program);
    inst.opr.src1.flags = decode_modifier(src1_mod);
    inst.opr.src1.index = 1;

    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, true, 7, m_second_program);
    inst.opr.src2.index = 2;

    if (src2_mod == 1) {
        inst.opr.src2.flags = RegisterFlags::Absolute;
    }

    inst.opr.dest.type = is_32_bit ? DataType::F32 : DataType::F16;
    inst.opr.src1.type = inst.opr.dest.type;
    inst.opr.src2.type = inst.opr.dest.type;

    const auto src1_swizzle_enc = src1_swiz_0_6 | src1_swiz_7_8 << 7 | src1_swiz_9 << 9 | src1_swiz_10_11 << 10;
    inst.opr.src1.swizzle = decode_swizzle4(src1_swizzle_enc);

    static const Swizzle4 tb_decode_src2_swizzle[] = {
        SWIZZLE_CHANNEL_4(X, X, X, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, Y),
        SWIZZLE_CHANNEL_4(Z, Z, Z, Z),
        SWIZZLE_CHANNEL_4(W, W, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, W),
        SWIZZLE_CHANNEL_4(Y, Z, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, Z),
        SWIZZLE_CHANNEL_4(X, X, Y, Z),
        SWIZZLE_CHANNEL_4(X, Y, X, Y),
        SWIZZLE_CHANNEL_4(X, Y, W, Z),
        SWIZZLE_CHANNEL_4(Z, X, Y, W),
        SWIZZLE_CHANNEL_4(Z, W, Z, W),
        SWIZZLE_CHANNEL_4(Y, Z, X, Z),
        SWIZZLE_CHANNEL_4(X, X, Y, Y),
        SWIZZLE_CHANNEL_4(X, Z, W, W),
        SWIZZLE_CHANNEL_4(X, Y, Z, 1),
    };

    inst.opr.src2.swizzle = tb_decode_src2_swizzle[src2_swiz];

    // TODO: source modifiers

    auto dest_comp_count = dest_mask_to_comp_count(dest_mask);

    LOG_DISASM("{:016x}: {}{} {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(opcode), disasm::operand_to_str(inst.opr.dest, dest_mask),
        disasm::operand_to_str(inst.opr.src1, dest_mask), disasm::operand_to_str(inst.opr.src2, dest_mask));

    // Recompile
    m_b.setLine(m_recompiler.cur_pc);

    inst.opcode = opcode;
    spv::Id result = do_alu_op(inst, dest_mask);

    if (result != spv::NoResult) {
        store(inst.opr.dest, result, dest_mask, 0);
    }

    return true;
}

bool USSETranslatorVisitor::vnmad16(
    ExtPredicate pred,
    bool skipinv,
    Imm2 src1_swiz_10_11,
    bool syncstart,
    Imm1 dest_bank_ext,
    Imm1 src1_swiz_9,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm4 src2_swiz,
    bool nosched,
    Imm4 dest_mask,
    Imm2 src1_mod,
    Imm1 src2_mod,
    Imm2 src1_swiz_7_8,
    Imm2 dest_bank_sel,
    Imm2 src1_bank_sel,
    Imm2 src2_bank_sel,
    Imm6 dest_n,
    Imm7 src1_swiz_0_6,
    Imm3 op2,
    Imm6 src1_n,
    Imm6 src2_n) {
    // TODO
    return vnmad32(pred, skipinv, src1_swiz_10_11, syncstart, dest_bank_ext, src1_swiz_9, src1_bank_ext, src2_bank_ext, src2_swiz, nosched, dest_mask, src1_mod, src2_mod, src1_swiz_7_8, dest_bank_sel, src1_bank_sel, src2_bank_sel, dest_n, src1_swiz_0_6, op2, src1_n, src2_n);
}

bool USSETranslatorVisitor::vbw(
    Imm3 op1,
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 nosched,
    Imm1 repeat_count,
    Imm1 sync_start,
    Imm1 dest_ext,
    Imm1 end,
    Imm1 src1_ext,
    Imm1 src2_ext,
    Imm4 mask_count,
    Imm1 src2_invert,
    Imm5 src2_rot,
    Imm2 src2_exth,
    Imm1 op2,
    Imm1 bitwise_partial,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src2_sel,
    Imm7 src1_n,
    Imm7 src2_n) {
    Instruction inst{};

    switch (op1) {
    case 0b010: inst.opcode = op2 ? Opcode::OR : Opcode::AND; break;
    case 0b011: inst.opcode = Opcode::XOR; break;
    case 0b100: inst.opcode = op2 ? Opcode::ROL : Opcode::SHL; break;
    case 0b101: inst.opcode = op2 ? Opcode::ASR : Opcode::SHR; break;
    default: return false;
    }

    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_ext, false, 8, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_ext, false, 8, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_ext, false, 8, m_second_program);

    spv::Id src1 = load(inst.opr.src1, 0b0001);
    spv::Id src2 = 0;

    bool immediate = src2_ext && inst.opr.src2.bank == RegisterBank::IMMEDIATE;
    uint32_t value = 0;

    if (src2_rot) {
        LOG_WARN("Bitwise Rotations are unsupported.");
        return false;
    }
    if (immediate) {
        value = src2_n | (static_cast<uint32_t>(src2_sel) << 7) | (static_cast<uint32_t>(src2_exth) << 14);
        src2 = m_b.makeUintConstant(src2_invert ? ~value : value);
    } else {
        src2 = load(inst.opr.src2, 0b0001);
        if (m_b.isFloatType(m_b.getTypeId(src2))) {
            src2 = m_b.createUnaryOp(spv::Op::OpBitcast, type_ui32, src2);
        }
        if (src2_invert) {
            src2 = m_b.createUnaryOp(spv::Op::OpNot, type_ui32, src2);
        }
    }

    spv::Id result;

    spv::Op operation;
    switch (inst.opcode) {
    case Opcode::OR: operation = spv::Op::OpBitwiseOr; break;
    case Opcode::AND: operation = spv::Op::OpBitwiseAnd; break;
    case Opcode::XOR: operation = spv::Op::OpBitwiseXor; break;
    case Opcode::ROL:
        LOG_WARN("Bitwise Rotate Left operation unsupported.");
        return false; // TODO: SPIRV doesn't seem to have a rotate left operation!
    case Opcode::ASR: operation = spv::Op::OpShiftRightArithmetic; break;
    case Opcode::SHL: operation = spv::Op::OpShiftLeftLogical; break;
    case Opcode::SHR: operation = spv::Op::OpShiftRightLogical; break;
    default: return false;
    }

    result = m_b.createBinOp(operation, type_ui32, src1, src2);
    if (m_b.isFloatType(m_b.getTypeId(src2))) {
        result = m_b.createUnaryOp(spv::Op::OpBitcast, type_f32, src2);
    }

    store(inst.opr.dest, result, 0b0001);

    LOG_DISASM("{:016x}: {}{} {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(inst.opcode),
        disasm::operand_to_str(inst.opr.src1, 0b0001),
        immediate ? log_hex(value) : disasm::operand_to_str(inst.opr.src2, 0b0001),
        disasm::operand_to_str(inst.opr.dest, 0b0001));

    return true;
}

bool USSETranslatorVisitor::vcomp(
    ExtPredicate pred,
    bool skipinv,
    Imm2 dest_type,
    bool syncstart,
    bool dest_bank_ext,
    bool end,
    bool src1_bank_ext,
    RepeatCount repeat_count,
    bool nosched,
    Imm2 op2,
    Imm2 src_type,
    Imm2 src1_mod,
    Imm2 src_comp,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm7 dest_n,
    Imm7 src1_n,
    Imm4 write_mask) {
    Instruction inst;

    // All of them needs to be doubled
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_bank_ext, true, 8, m_second_program);
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, true, 8, m_second_program);
    inst.opr.src1.flags = decode_modifier(src1_mod);

    // The thing is, these instructions are designed to only work with one component
    // I'm pretty sure, but should leave note here in the future if other devs maintain and develop this.
    static const Opcode tb_decode_vop[] = {
        Opcode::VRCP,
        Opcode::VRSQ,
        Opcode::VLOG,
        Opcode::VEXP
    };

    static const DataType tb_decode_src_type[] = {
        DataType::F32,
        DataType::F16,
        DataType::C10,
        DataType::UNK
    };

    static const DataType tb_decode_dest_type[] = {
        DataType::F32,
        DataType::F16,
        DataType::C10,
        DataType::UNK
    };

    const Opcode op = tb_decode_vop[op2];
    inst.opr.src1.type = tb_decode_src_type[src_type];
    inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.dest.type = tb_decode_dest_type[dest_type];
    inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    // TODO: Should we do this ?
    write_mask = decode_write_mask(write_mask, inst.opr.src1.type == DataType::F16);

    std::uint32_t src_mask = 0;

    // Build the source mask. It should only be one component
    switch (src_comp) {
    case 0: {
        src_mask = 0b0001;
        break;
    }

    case 1: {
        src_mask = 0b0010;
        break;
    }

    case 2: {
        src_mask = 0b0100;
        break;
    }

    case 3: {
        src_mask = 0b1000;
        break;
    }

    default: break;
    }

    m_b.setLine(m_recompiler.cur_pc);

    // TODO: Log
    BEGIN_REPEAT(repeat_count, 2)
    GET_REPEAT(inst);
    spv::Id result = load(inst.opr.src1, src_mask, src1_repeat_offset);

    switch (op) {
    case Opcode::VRCP: {
        // We have to manually divide by 1
        const int num_comp = m_b.getNumComponents(result);
        const spv::Id one_const = m_b.makeFloatConstant(1.0f);
        spv::Id one_v = spv::NoResult;

        if (num_comp == 1) {
            one_v = one_const;
        } else {
            std::vector<spv::Id> ones;
            ones.insert(ones.begin(), num_comp, one_const);

            one_v = m_b.makeCompositeConstant(type_f32_v[num_comp], ones);
        }

        result = m_b.createBinOp(spv::OpFDiv, m_b.getTypeId(result), one_v, result);
        break;
    }

    case Opcode::VRSQ: {
        // Inverse squareroot. Call a builtin this case.
        result = m_b.createBuiltinCall(m_b.getTypeId(result), std_builtins, GLSLstd450InverseSqrt, { result });
        break;
    }

    case Opcode::VLOG: {
        // src0 = e^y => return y
        result = m_b.createBuiltinCall(m_b.getTypeId(result), std_builtins, GLSLstd450Log, { result });
        break;
    }

    case Opcode::VEXP: {
        // y = e^src0 => return y
        result = m_b.createBuiltinCall(m_b.getTypeId(result), std_builtins, GLSLstd450Exp, { result });
        break;
    }

    default:
        break;
    }

    store(inst.opr.dest, result, write_mask, dest_repeat_offset);

    LOG_DISASM("{:016x}: {}{} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(op), disasm::operand_to_str(inst.opr.src1, src_mask, src1_repeat_offset),
        disasm::operand_to_str(inst.opr.dest, write_mask, dest_repeat_offset));
    END_REPEAT()

    return true;
}
