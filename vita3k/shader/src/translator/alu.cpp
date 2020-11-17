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
    RepeatMode repeat_mode,
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

    Instruction inst;

    // Is this VMAD3 or VMAD4, op2 = 0 => vec3
    int type = 2;

    if (opcode2 == 0) {
        type = 1;
    }

    // Double regs always true for src1, dest
    inst.opr.src1 = decode_src12(inst.opr.src1, src0_n, src0_bank, src0_bank_ext, true, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_use_bank_ext, true, 7, m_second_program);

    // GPI0 and GPI1, setup!
    inst.opr.src0.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src0.num = gpi0_n;

    inst.opr.src2.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src2.num = gpi1_n;

    // Swizzleee
    if (type == 1) {
        inst.opr.dest.swizzle[3] = usse::SwizzleChannel::_X;
    }

    inst.opr.src1.swizzle = decode_vec34_swizzle(src0_swiz, src0_swiz_ext, type);
    inst.opr.src0.swizzle = decode_vec34_swizzle(gpi0_swiz, gpi0_swiz_ext, type);
    inst.opr.src2.swizzle = decode_vec34_swizzle(gpi1_swiz, gpi1_swiz_ext, type);

    if (src0_abs) {
        inst.opr.src1.flags |= RegisterFlags::Absolute;
    }

    if (src0_neg) {
        inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    if (gpi0_abs) {
        inst.opr.src0.flags |= RegisterFlags::Absolute;
    }

    if (gpi0_neg) {
        inst.opr.src0.flags |= RegisterFlags::Negative;
    }

    if (gpi1_abs) {
        inst.opr.src2.flags |= RegisterFlags::Absolute;
    }

    if (gpi1_neg) {
        inst.opr.src2.flags |= RegisterFlags::Negative;
    }

    m_b.setLine(m_recompiler.cur_pc);

    // Write mask is a 4-bit immidiate
    // If a bit is one, a swizzle is active
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(inst, repeat_mode);

    LOG_DISASM("{} {} {} {} {}", disasm_str, disasm::operand_to_str(inst.opr.dest, write_mask), disasm::operand_to_str(inst.opr.src0, write_mask, src0_repeat_offset), disasm::operand_to_str(inst.opr.src1, write_mask, src1_repeat_offset),
        disasm::operand_to_str(inst.opr.src2, write_mask, src2_repeat_offset));

    spv::Id vsrc0 = load(inst.opr.src0, write_mask, src0_repeat_offset);
    spv::Id vsrc1 = load(inst.opr.src1, write_mask, src1_repeat_offset);
    spv::Id vsrc2 = load(inst.opr.src2, write_mask, src2_repeat_offset);

    if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        LOG_ERROR("Source not loaded (vsrc0: {}, vsr1: {}, vsrc2: {})", vsrc0, vsrc1, vsrc2);
        return false;
    }

    auto mul_result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(vsrc0), vsrc0, vsrc1);
    auto add_result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(mul_result), mul_result, vsrc2);

    store(inst.opr.dest, add_result, write_mask, 0);
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
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
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
    Instruction inst;
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
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, true, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, true, 7, m_second_program);

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

    const std::uint8_t src0_swiz = src0_swiz_bits01 | src0_swiz_bits2 << 2;
    const std::uint8_t src1_swiz = src1_swiz_bits01 | src1_swiz_bit2 << 2;

    inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    inst.opr.src0.swizzle = tb_decode_src0_swiz[src0_swiz];
    inst.opr.src1.swizzle = tb_decode_src1_swiz[src1_swiz];
    inst.opr.src2.swizzle = tb_decode_src2_swiz[src2_swiz];

    // Decode modifiers
    if (src0_abs) {
        inst.opr.src0.flags |= RegisterFlags::Absolute;
    }

    inst.opr.src1.flags = decode_modifier(src1_mod);
    inst.opr.src2.flags = decode_modifier(src2_mod);

    // Log the instruction
    LOG_DISASM("{:016x}: {}{}2 {} {} {} {}", m_instr, disasm::e_predicate_str(static_cast<ExtPredicate>(pred)), disasm::opcode_str(op), disasm::operand_to_str(inst.opr.dest, dest_mask),
        disasm::operand_to_str(inst.opr.src0, dest_mask), disasm::operand_to_str(inst.opr.src1, dest_mask), disasm::operand_to_str(inst.opr.src2, dest_mask));

    m_b.setLine(m_recompiler.cur_pc);

    // Translate the instruction
    spv::Id vsrc0 = load(inst.opr.src0, dest_mask, 0);
    spv::Id vsrc1 = load(inst.opr.src1, dest_mask, 0);
    spv::Id vsrc2 = load(inst.opr.src2, dest_mask, 0);

    if (vsrc0 == spv::NoResult || vsrc1 == spv::NoResult || vsrc2 == spv::NoResult) {
        LOG_ERROR("Source not loaded (vsrc0: {}, vsr1: {}, vsrc2: {})", vsrc0, vsrc1, vsrc2);
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
    RepeatMode repeat_mode,
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

    // Double regs always true for src1, dest
    // src0 is actually src1
    // src1 is gpi0, which repeat offset can't affect to
    inst.opr.src1 = decode_src12(inst.opr.src1, src0_n, src0_bank, src0_bank_ext, true, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_use_bank_ext, true, 7, m_second_program);

    inst.opr.src0.bank = usse::RegisterBank::FPINTERNAL;
    inst.opr.src0.num = gpi0_n;
    inst.opr.src0.swizzle = decode_vec34_swizzle(gpi0_swiz, false, type);

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

    inst.opr.src1.swizzle[0] = tb_swizz_dec[src0_swiz_x];
    inst.opr.src1.swizzle[1] = tb_swizz_dec[src0_swiz_y];
    inst.opr.src1.swizzle[2] = tb_swizz_dec[src0_swiz_z];
    inst.opr.src1.swizzle[3] = tb_swizz_dec[src0_swiz_w];

    // Set modifiers
    if (src0_neg) {
        inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    if (src0_abs) {
        inst.opr.src1.flags |= RegisterFlags::Absolute;
    }

    if (gpi0_abs) {
        inst.opr.src0.flags |= RegisterFlags::Absolute;
    }

    m_b.setLine(m_recompiler.cur_pc);

    // Decoding done
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(inst, repeat_mode)

    LOG_DISASM("{:016x}: {}VDP {} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::operand_to_str(inst.opr.dest, write_mask, current_repeat),
        disasm::operand_to_str(inst.opr.src0, src_mask, src0_repeat_offset), disasm::operand_to_str(inst.opr.src1, src_mask, src1_repeat_offset));

    spv::Id lhs = load(inst.opr.src0, type == 1 ? 0b0111 : 0b1111, src0_repeat_offset);
    spv::Id rhs = load(inst.opr.src1, type == 1 ? 0b0111 : 0b1111, src1_repeat_offset);

    if (lhs == spv::NoResult || rhs == spv::NoResult) {
        LOG_ERROR("Source not loaded (lhs: {}, rhs: {})", lhs, rhs);
        return false;
    }

    spv::Id result = m_b.createBinOp(spv::OpDot, type_f32, lhs, rhs);

    store(inst.opr.dest, result, write_mask, current_repeat);
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
    case Opcode::AND: {
        result = m_b.createBinOp(spv::OpBitwiseAnd, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::OR: {
        result = m_b.createBinOp(spv::OpBitwiseOr, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::XOR: {
        result = m_b.createBinOp(spv::OpBitwiseXor, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::SHL: {
        result = m_b.createBinOp(spv::OpShiftLeftLogical, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::SHR: {
        result = m_b.createBinOp(spv::OpShiftRightLogical, source_type, vsrc1, vsrc2);
        break;
    }

    case Opcode::ASR: {
        result = m_b.createBinOp(spv::OpShiftRightArithmetic, source_type, vsrc1, vsrc2);
        break;
    }

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

    case Opcode::VDP:
    case Opcode::VF16DP: {
        result = m_b.createBinOp(spv::OpDot, m_b.makeFloatType(32), vsrc1, vsrc2);
        break;
    }

    case Opcode::ISUBU32:
    case Opcode::ISUB32: {
        result = m_b.createBinOp(spv::OpISub, m_b.makeIntType(32), vsrc1, vsrc2);
        break;
    }

    default: {
        LOG_ERROR("Unimplemented ALU opcode instruction: {}", disasm::opcode_str(inst.opcode));
        break;
    }
    }

    return result;
}

bool USSETranslatorVisitor::v32nmad(
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
    Instruction inst;

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
    inst.opr.src2.swizzle = decode_vec34_swizzle(src2_swiz, false, 2);

    // TODO: source modifiers

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

bool USSETranslatorVisitor::v16nmad(
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
    return v32nmad(pred, skipinv, src1_swiz_10_11, syncstart, dest_bank_ext, src1_swiz_9, src1_bank_ext, src2_bank_ext, src2_swiz, nosched, dest_mask, src1_mod, src2_mod, src1_swiz_7_8, dest_bank_sel, src1_bank_sel, src2_bank_sel, dest_n, src1_swiz_0_6, op2, src1_n, src2_n);
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
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(inst, RepeatMode::SLMSI);
    spv::Id result = load(inst.opr.src1, src_mask, src1_repeat_offset);

    if (result == spv::NoResult) {
        LOG_ERROR("Result not loaded");
        return false;
    }

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

    LOG_DISASM("{:016x}: {}{} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(op), disasm::operand_to_str(inst.opr.dest, write_mask, dest_repeat_offset),
        disasm::operand_to_str(inst.opr.src1, src_mask, src1_repeat_offset));
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::sop(
    Imm2 pred,
    Imm1 cmod1,
    Imm1 skipinv,
    Imm1 nosched,
    Imm2 asel1,
    Imm1 dest_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    Imm1 src2_bank_ext,
    Imm1 cmod2,
    Imm3 count,
    Imm1 amod1,
    Imm2 asel2,
    Imm3 csel1,
    Imm3 csel2,
    Imm1 amod2,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm1 src1_mod,
    Imm2 cop,
    Imm2 aop,
    Imm1 asrc1_mod,
    Imm1 dest_mod,
    Imm7 src1_n,
    Imm7 src2_n) {
    static auto selector_zero = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                    const spv::Id src2_alpha) {
        return utils::make_uniform_vector_from_type(b, type, 0);
    };

    static auto selector_src1_color = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                          const spv::Id src2_alpha) {
        return src1_color;
    };

    static auto selector_src2_color = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                          const spv::Id src2_alpha) {
        return src2_color;
    };

    static auto selector_src1_alpha = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                          const spv::Id src2_alpha) {
        if (!b.isScalarType(type) || b.getNumTypeComponents(type) > 1) {
            // We must do a composite construct
            return b.createCompositeConstruct(type, { src1_alpha, src1_alpha, src1_alpha });
        }

        return src1_alpha;
    };

    static auto selector_src2_alpha = [](spv::Builder &b, const spv::Id type, const spv::Id src1_color, const spv::Id src2_color, const spv::Id src1_alpha,
                                          const spv::Id src2_alpha) {
        if (!b.isScalarType(type) || b.getNumTypeComponents(type) > 1) {
            // We must do a composite construct
            return b.createCompositeConstruct(type, { src2_alpha, src2_alpha, src2_alpha });
        }

        return src2_alpha;
    };

    // This opcode always operates on C10.
    static Opcode operations[] = {
        Opcode::FADD,
        Opcode::FSUB,
        Opcode::FMIN,
        Opcode::FMAX
    };

    using SelectorFunc = std::function<spv::Id(spv::Builder &, const spv::Id, const spv::Id, const spv::Id,
        const spv::Id, const spv::Id)>;

    static SelectorFunc color_selector_1[] = {
        selector_zero,
        selector_src1_color,
        selector_src2_color,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr, // source alpha saturated.
        nullptr // source alpha scale
    };

    static SelectorFunc color_selector_2[] = {
        selector_zero,
        selector_src1_color,
        selector_src2_color,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr, // source alpha saturated.
        nullptr // zero source 2 minus half.
    };

    static SelectorFunc alpha_selector_1[] = {
        selector_zero,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr // source 2 scale.
    };

    static SelectorFunc alpha_selector_2[] = {
        selector_zero,
        selector_src1_alpha,
        selector_src2_alpha,
        nullptr // zero source 2 minus half.
    };

    Instruction inst;
    inst.opcode = Opcode::SOP2;

    // Never double register num
    inst.opr.src1 = decode_src12(inst.opr.src1, src1_n, src1_bank, src1_bank_ext, false, 7, m_second_program);
    inst.opr.src2 = decode_src12(inst.opr.src2, src2_n, src2_bank, src2_bank_ext, false, 7, m_second_program);
    inst.opr.dest = decode_dest(inst.opr.dest, dest_n, dest_bank, dest_bank_ext, false, 7, m_second_program);

    inst.opr.src1.type = DataType::UINT8;
    inst.opr.src2.type = DataType::UINT8;
    inst.opr.dest.type = DataType::UINT8;

    if (cop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid color opcode: {}", (int)cop);
        return true;
    }

    if (aop >= sizeof(operations) / sizeof(Opcode)) {
        LOG_ERROR("Invalid alpha opcode: {}", (int)aop);
        return true;
    }

    Opcode color_op = operations[cop];
    Opcode alpha_op = operations[aop];

    if (csel1 >= sizeof(color_selector_1) / sizeof(SelectorFunc) || csel2 >= sizeof(color_selector_2) / sizeof(SelectorFunc) || asel1 >= sizeof(alpha_selector_1) / sizeof(SelectorFunc) || asel2 >= sizeof(alpha_selector_2) / sizeof(SelectorFunc)) {
        LOG_ERROR("Unknown color/alpha selector (csel1: {}, csel2: {}, asel1: {}, asel2: {}", csel1, csel2,
            asel1, asel2);
        return true;
    }

    // Lookup color selector
    SelectorFunc color_selector_1_func = color_selector_1[csel1];
    SelectorFunc color_selector_2_func = color_selector_2[csel2];
    SelectorFunc alpha_selector_1_func = alpha_selector_1[asel1];
    SelectorFunc alpha_selector_2_func = alpha_selector_2[asel2];

    if (!color_selector_1_func || !color_selector_2_func || !alpha_selector_1_func || !alpha_selector_2_func) {
        LOG_ERROR("Unimplemented color/alpha selector (csel1: {}, csel2: {}, asel1: {}, asel2: {}", csel1, csel2,
            asel1, asel2);
        return true;
    }

    auto apply_opcode = [&](Opcode op, spv::Id type, spv::Id lhs, spv::Id rhs) {
        switch (op) {
        case Opcode::FADD: {
            return m_b.createBinOp(spv::OpFAdd, type, lhs, rhs);
        }

        case Opcode::FSUB: {
            return m_b.createBinOp(spv::OpFSub, type, lhs, rhs);
        }

        case Opcode::FMIN:
        case Opcode::VMIN: {
            return m_b.createBuiltinCall(type, std_builtins, GLSLstd450FMin, { lhs, rhs });
        }

        case Opcode::FMAX:
        case Opcode::VMAX: {
            return m_b.createBuiltinCall(type, std_builtins, GLSLstd450FMax, { lhs, rhs });
        }

        default: {
            LOG_ERROR("Unsuppported sop opcode: {}", disasm::opcode_str(op));
            break;
        }
        }

        return spv::NoResult;
    };

    spv::Id uniform_1_alpha = spv::NoResult;
    spv::Id uniform_1_color = spv::NoResult;

    auto apply_complement_modifiers = [&](spv::Id &target, Imm1 enable, spv::Id type, const bool is_rgb) {
        if (enable == 1) {
            if (is_rgb) {
                if (!uniform_1_color)
                    uniform_1_color = utils::make_uniform_vector_from_type(m_b, type, 1);

                target = m_b.createBinOp(spv::OpFSub, type, uniform_1_color, target);
            } else {
                if (!uniform_1_alpha)
                    uniform_1_alpha = m_b.makeFloatConstant(1.0f);

                target = m_b.createBinOp(spv::OpFSub, type, uniform_1_alpha, target);
            }
        }
    };

    BEGIN_REPEAT(count)
    GET_REPEAT(inst, RepeatMode::SLMSI)

    spv::Id src1_color = load(inst.opr.src1, 0b0111, src1_repeat_offset);
    spv::Id src2_color = load(inst.opr.src2, 0b0111, src2_repeat_offset);
    spv::Id src1_alpha = load(inst.opr.src1, 0b1000, src1_repeat_offset);
    spv::Id src2_alpha = load(inst.opr.src2, 0b1000, src2_repeat_offset);

    src1_color = utils::convert_to_float(m_b, src1_color, DataType::UINT8, true);
    src2_color = utils::convert_to_float(m_b, src2_color, DataType::UINT8, true);
    src1_alpha = utils::convert_to_float(m_b, src1_alpha, DataType::UINT8, true);
    src2_alpha = utils::convert_to_float(m_b, src2_alpha, DataType::UINT8, true);

    spv::Id src_color_type = m_b.getTypeId(src1_color);
    spv::Id src_alpha_type = m_b.getTypeId(src1_alpha);

    // Apply source flag.
    // Complement is the vector that when added with source creates a nice vec4(1.0).
    // Since the whole opcode is around stencil and blending.
    apply_complement_modifiers(src1_color, src1_mod, src_color_type, true);
    apply_complement_modifiers(src1_alpha, src1_mod, src_alpha_type, false);

    // Apply color selector
    spv::Id factored_rgb_lhs = color_selector_1_func(m_b, src_color_type, src1_color, src2_color, src1_alpha, src2_alpha);
    spv::Id factored_rgb_rhs = color_selector_2_func(m_b, src_color_type, src1_color, src2_color, src1_alpha, src2_alpha);

    spv::Id factored_a_lhs = alpha_selector_1_func(m_b, src_alpha_type, src1_color, src2_color, src1_alpha, src2_alpha);
    spv::Id factored_a_rhs = alpha_selector_2_func(m_b, src_alpha_type, src1_color, src2_color, src1_alpha, src2_alpha);

    // Apply modifiers
    // BREAKDOWN BREAKDOWN
    apply_complement_modifiers(factored_rgb_lhs, cmod1, src_color_type, true);
    apply_complement_modifiers(factored_rgb_rhs, cmod2, src_color_type, true);
    apply_complement_modifiers(factored_a_lhs, amod1, src_alpha_type, false);
    apply_complement_modifiers(factored_a_rhs, amod2, src_alpha_type, false);

    // Factor them with source
    factored_rgb_lhs = m_b.createBinOp(spv::OpFMul, src_color_type, factored_rgb_lhs, src1_color);
    factored_rgb_rhs = m_b.createBinOp(spv::OpFMul, src_color_type, factored_rgb_rhs, src2_color);

    factored_a_lhs = m_b.createBinOp(spv::OpFMul, src_color_type, factored_a_lhs, src1_alpha);
    factored_a_rhs = m_b.createBinOp(spv::OpFMul, src_color_type, factored_a_rhs, src2_alpha);

    auto color_res = apply_opcode(color_op, src_color_type, factored_rgb_lhs, factored_rgb_rhs);
    auto alpha_res = apply_opcode(alpha_op, src_alpha_type, factored_a_lhs, factored_a_rhs);

    color_res = utils::convert_to_int(m_b, color_res, DataType::UINT8, true);
    alpha_res = utils::convert_to_int(m_b, alpha_res, DataType::UINT8, true);

    // Final result. Do binary operation and then store
    store(inst.opr.dest, color_res, 0b0111, dest_repeat_offset);
    store(inst.opr.dest, alpha_res, 0b1000, dest_repeat_offset);

    // TODO log correctly
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(color_op));
    LOG_DISASM("{:016x}: {}", m_instr, disasm::opcode_str(alpha_op));

    END_REPEAT()

    return true;
}

bool shader::usse::USSETranslatorVisitor::sop2() {
    LOG_DISASM("Unimplmenet Opcode: sop2");
    return true;
}

bool shader::usse::USSETranslatorVisitor::sop3() {
    LOG_DISASM("Unimplmenet Opcode: sop3");
    return true;
}

enum class DualSrcId {
    INTERAL0,
    INTERNAL1,
    INTERAL2,
    UNIFIED,
    NONE,
};

typedef std::array<DualSrcId, 3> DualSrcLayout;

static optional<DualSrcLayout> get_dual_op1_src_layout(uint8_t count, Imm2 config) {
    switch (count) {
    case 1:
        switch (config) {
        case 0: return optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::NONE, DualSrcId::NONE });
        case 1: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::NONE, DualSrcId::NONE });
        case 2: return optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::NONE, DualSrcId::NONE });
        case 3: return optional<DualSrcLayout>({ DualSrcId::INTERAL2, DualSrcId::NONE, DualSrcId::NONE });
        default:
            return {};
        }
    case 2:
        switch (config) {
        case 0: return optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::INTERNAL1, DualSrcId::NONE });
        case 1: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::NONE });
        case 2: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::NONE });
        default:
            return {};
        }
    case 3:
        switch (config) {
        case 0: return optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::INTERNAL1, DualSrcId::INTERAL2 });
        case 1: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::INTERAL2 });
        case 2: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::UNIFIED });
        case 3: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::INTERAL2 });
        default:
            return {};
        }
    default:
        return {};
    }

    return {};
}

// Dual op2 layout depends on op1's src layout too...
static optional<DualSrcLayout> get_dual_op2_src_layout(uint8_t op1_count, uint8_t op2_count, Imm2 src_config) {
    switch (op1_count) {
    case 1:
        switch (op2_count) {
        case 1:
            switch (src_config) {
            case 0: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::NONE, DualSrcId::NONE });
            case 1:
            case 2:
            case 3:
                return optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::NONE, DualSrcId::NONE });
            default: return {};
            }
        case 2:
            switch (src_config) {
            case 0: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::NONE });
            case 1: return optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::INTERNAL1, DualSrcId::NONE });
            case 2:
            case 3:
                return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::NONE });
            default: return {};
            }
        case 3:
            switch (src_config) {
            case 0: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::INTERAL2 });
            case 1: return optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::INTERNAL1, DualSrcId::INTERAL2 });
            case 2: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::INTERAL2 });
            case 3: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::INTERNAL1 });
            default: return {};
            }
        default: return {};
        }
    case 2:
        switch (op2_count) {
        case 1:
            switch (src_config) {
            case 0: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::NONE, DualSrcId::NONE });
            case 1: return optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::NONE, DualSrcId::NONE });
            case 2: return optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::NONE, DualSrcId::NONE });
            default: return {};
            }
        case 2:
            switch (src_config) {
            case 0: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERAL2, DualSrcId::NONE });
            case 1: return optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::INTERAL2, DualSrcId::NONE });
            case 2: return optional<DualSrcLayout>({ DualSrcId::INTERAL2, DualSrcId::UNIFIED, DualSrcId::NONE });
            default: return {};
            }
        default: return {};
        }
    case 3:
        switch (op2_count) {
        case 1:
            switch (src_config) {
            case 0: return optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::NONE, DualSrcId::NONE });
            case 1: return optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::NONE, DualSrcId::NONE });
            case 2: return optional<DualSrcLayout>({ DualSrcId::INTERAL2, DualSrcId::NONE, DualSrcId::NONE });
            case 3: return optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::NONE, DualSrcId::NONE });
            default: return {};
            }
        default: return {};
        }
    default: return {};
    }

    return {};
}

bool USSETranslatorVisitor::vdual(
    Imm1 comp_count_type,
    Imm1 gpi1_neg,
    Imm2 sv_pred,
    Imm1 skipinv,
    Imm1 dual_op1_ext_vec3_or_has_w_vec4,
    bool type_f16,
    Imm1 gpi1_swizz_ext,
    Imm4 unified_store_swizz,
    Imm1 unified_store_neg,
    Imm3 dual_op1,
    Imm1 dual_op2_ext,
    bool prim_ustore,
    Imm4 gpi0_swizz,
    Imm4 gpi1_swizz,
    Imm2 prim_dest_bank,
    Imm2 unified_store_slot_bank,
    Imm2 prim_dest_num_gpi_case,
    Imm7 prim_dest_num,
    Imm3 dual_op2,
    Imm2 src_config,
    Imm1 gpi2_slot_num_bit_1,
    Imm1 gpi2_slot_num_bit_0_or_unified_store_abs,
    Imm2 gpi1_slot_num,
    Imm2 gpi0_slot_num,
    Imm3 write_mask_non_gpi,
    Imm7 unified_store_slot_num) {
    Instruction op1;
    Instruction op2;

    const Opcode op1_codes[16] = {
        Opcode::VMAD,
        Opcode::VDP,
        Opcode::VSSQ,
        Opcode::VMUL,
        Opcode::VADD,
        Opcode::VMOV,
        Opcode::FRSQ,
        Opcode::FRCP,

        Opcode::FMAD,
        Opcode::FADD,
        Opcode::FMUL,
        Opcode::FSUBFLR,
        Opcode::FEXP,
        Opcode::FLOG,
        Opcode::INVALID,
        Opcode::INVALID,
    };

    const Opcode op2_codes[16] = {
        Opcode::INVALID,
        Opcode::VDP,
        Opcode::VSSQ,
        Opcode::VMUL,
        Opcode::VADD,
        Opcode::VMOV,
        Opcode::FRSQ,
        Opcode::FRCP,

        Opcode::FMAD,
        Opcode::FADD,
        Opcode::FMUL,
        Opcode::FSUBFLR,
        Opcode::FEXP,
        Opcode::FLOG,
        Opcode::INVALID,
        Opcode::INVALID,
    };

    // Each instruction might have a different source layout or write mask depending on how the instruction works.
    // Let's store insturction information in a map so it's easy for each instruction to be loaded.
    struct DualOpInfo {
        uint8_t src_count;
        bool vector_load;
        bool vector_store;
    };

    const std::map<Opcode, DualOpInfo> op_info = {
        { Opcode::VMAD, { 3, true, true } },
        { Opcode::VDP, { 2, true, false } },
        { Opcode::VSSQ, { 1, true, false } },
        { Opcode::VMUL, { 2, true, true } },
        { Opcode::VADD, { 2, true, true } },
        { Opcode::VMOV, { 1, true, true } },
        { Opcode::FRSQ, { 1, false, false } },
        { Opcode::FRCP, { 1, false, false } },
        { Opcode::FMAD, { 3, false, false } },
        { Opcode::FADD, { 2, false, false } },
        { Opcode::FMUL, { 2, false, false } },
        { Opcode::FSUBFLR, { 2, false, false } },
        { Opcode::FEXP, { 1, false, false } },
        { Opcode::FLOG, { 1, false, false } },
    };

    auto get_dual_op_write_mask = [&](const DualOpInfo &op, bool dest_internal) {
        // Write masks are complicated and seem to depend on if they are in internal regs. Please double check.
        if (dest_internal)
            return comp_count_type ? 0b1111 : 0b0111;
        if (op.vector_store)
            return type_f16 ? 0b1111 : 0b0011;
        else
            return type_f16 ? 0b0011 : 0b0001;
    };

    op1.opcode = op1_codes[(!comp_count_type && dual_op1_ext_vec3_or_has_w_vec4) << 3u | dual_op1];
    op2.opcode = op2_codes[dual_op2_ext << 3u | dual_op2];

    // Unified is only part of instruction that can reference any bank. Others are internal.
    Operand unified_dest;
    Operand internal_dest;
    unified_dest = decode_dest(unified_dest, prim_dest_num, prim_dest_bank, false, true, 8, m_second_program);
    internal_dest.bank = RegisterBank::FPINTERNAL;
    internal_dest.num = prim_dest_num_gpi_case;
    internal_dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    // op1 is primary instruction. op2 is secondary.
    op1.opr.dest = prim_ustore ? unified_dest : internal_dest;
    op2.opr.dest = prim_ustore ? internal_dest : unified_dest;

    const auto op1_info = op_info.at(op1.opcode);
    const auto op2_info = op_info.at(op2.opcode);
    const optional<DualSrcLayout> op1_layout = get_dual_op1_src_layout(op1_info.src_count, src_config);
    const optional<DualSrcLayout> op2_layout = get_dual_op2_src_layout(op1_info.src_count, op2_info.src_count, src_config);

    if (!op1_layout) {
        LOG_ERROR("Missing dual for op1 layout.");
        return false;
    }

    if (!op2_layout) {
        LOG_ERROR("Missing dual for op2 layout.");
        return false;
    }

    Imm4 fixed_write_mask = write_mask_non_gpi | ((comp_count_type && dual_op1_ext_vec3_or_has_w_vec4) << 3u);

    Imm4 op1_write_mask = prim_ustore ? get_dual_op_write_mask(op1_info, op1.opr.dest.bank == RegisterBank::FPINTERNAL) : fixed_write_mask;
    Imm4 op2_write_mask = prim_ustore ? fixed_write_mask : get_dual_op_write_mask(op2_info, op2.opr.dest.bank == RegisterBank::FPINTERNAL);

    auto create_srcs_from_layout = [&](std::array<DualSrcId, 3> layout, DualOpInfo code_info) {
        std::vector<Operand> srcs;

        for (DualSrcId id : layout) {
            if (id == DualSrcId::NONE)
                break;

            Operand op;

            switch (id) {
            case DualSrcId::UNIFIED:
                op = decode_src12(op, unified_store_slot_num, unified_store_slot_bank, false,
                    code_info.vector_load, code_info.vector_load ? 8 : 7, m_second_program);
                // gpi2_slot_num_bit_1 is also unified source ext
                op.swizzle = decode_dual_swizzle(unified_store_swizz,
                    code_info.src_count >= 2 ? false : gpi2_slot_num_bit_1, comp_count_type);
                if (gpi2_slot_num_bit_0_or_unified_store_abs)
                    op.flags |= RegisterFlags::Absolute;
                if (unified_store_neg)
                    op.flags |= RegisterFlags::Negative;
                break;
            case DualSrcId::INTERAL0:
                op.bank = RegisterBank::FPINTERNAL;
                op.num = gpi0_slot_num;
                op.swizzle = decode_dual_swizzle(gpi0_swizz, false, comp_count_type);
                break;
            case DualSrcId::INTERNAL1:
                op.bank = RegisterBank::FPINTERNAL;
                op.num = gpi1_slot_num;
                op.swizzle = decode_dual_swizzle(gpi1_swizz, gpi1_swizz_ext, comp_count_type);
                if (gpi1_neg)
                    op.flags |= RegisterFlags::Negative;
                break;
            case DualSrcId::INTERAL2:
                op.bank = RegisterBank::FPINTERNAL;
                op.num = code_info.src_count >= 2 ? (gpi2_slot_num_bit_1 << 1u | gpi2_slot_num_bit_0_or_unified_store_abs) : 2;
                op.swizzle = SWIZZLE_CHANNEL_4(X, Y, Z, W);
                break;
            default:
                break;
            }
            op.type = type_f16 ? DataType::F16 : DataType::F32;

            srcs.push_back(op);
        }

        return srcs;
    };

    std::vector<Operand> op1_srcs = create_srcs_from_layout(*op1_layout, op1_info);
    std::vector<Operand> op2_srcs = create_srcs_from_layout(*op2_layout, op2_info);

    std::string disasm_str = fmt::format("{:016x}: ", m_instr);

    auto do_dual_op = [&](Opcode code, std::vector<Operand> &ops, Operand &dest,
                          Imm4 write_mask_dest, const DualOpInfo &code_info) {
        uint32_t write_mask_source = code_info.vector_load ? (0b1111u >> !comp_count_type) : 0b0001;

        spv::Id result;

        switch (code) {
        case Opcode::VMOV: {
            result = load(ops[0], write_mask_source);
            break;
        }
        case Opcode::VADD: {
            const spv::Id first = load(ops[0], write_mask_source);
            const spv::Id second = load(ops[1], write_mask_source);
            result = m_b.createBinOp(spv::OpFAdd, m_b.getTypeId(first), first, second);
            break;
        }
        case Opcode::FRCP: {
            const spv::Id source = load(ops[0], write_mask_source);
            const int num_comp = m_b.getNumComponents(source);
            const spv::Id one_const = m_b.makeFloatConstant(1.0f);
            spv::Id one_v = spv::NoResult;

            if (num_comp == 1) {
                one_v = one_const;
            } else {
                std::vector<spv::Id> ones;
                ones.insert(ones.begin(), num_comp, one_const);

                one_v = m_b.makeCompositeConstant(type_f32_v[num_comp], ones);
            }

            result = m_b.createBinOp(spv::OpFDiv, m_b.getTypeId(source), one_v, source);
            break;
        }
        case Opcode::FRSQ: {
            const spv::Id source = load(ops[0], write_mask_source);
            result = m_b.createBuiltinCall(m_b.getTypeId(source), std_builtins, GLSLstd450InverseSqrt, { source });
            break;
        }
        case Opcode::FMUL:
        case Opcode::VMUL: {
            const spv::Id first = load(ops[0], write_mask_source);
            const spv::Id second = load(ops[1], write_mask_source);
            result = m_b.createBinOp(spv::OpFMul, m_b.getTypeId(first), first, second);
            break;
        }
        case Opcode::VDP: {
            const spv::Id first = load(ops[0], write_mask_source);
            const spv::Id second = load(ops[1], write_mask_source);
            result = m_b.createBinOp(spv::OpDot, type_f32, first, second);
            break;
        }
        case Opcode::FEXP: {
            const spv::Id source = load(ops[0], write_mask_source);
            result = m_b.createBuiltinCall(m_b.getTypeId(source), std_builtins, GLSLstd450Exp, { source });
            break;
        }
        case Opcode::FLOG: {
            const spv::Id source = load(ops[0], write_mask_source);
            result = m_b.createBuiltinCall(m_b.getTypeId(source), std_builtins, GLSLstd450Log, { source });
            break;
        }
        case Opcode::VSSQ: {
            const spv::Id source = load(ops[0], write_mask_source);
            result = m_b.createBinOp(spv::OpDot, type_f32, source, source);
            break;
        }
        case Opcode::FMAD:
        case Opcode::VMAD: {
            const spv::Id first = load(ops[0], write_mask_source);
            const spv::Id second = load(ops[1], write_mask_source);
            const spv::Id third = load(ops[2], write_mask_source);
            const spv::Id type = m_b.getTypeId(first);
            result = m_b.createBinOp(spv::OpFMul, type, first, second);
            result = m_b.createBinOp(spv::OpFAdd, type, result, third);
            break;
        }
        default:
            LOG_ERROR("Missing implementation for DUAL {}.", disasm::opcode_str(code));
            return spv::NoResult;
        }

        disasm_str += fmt::format("{} {}", disasm::opcode_str(code), disasm::operand_to_str(dest, write_mask_dest));

        for (Operand &op : ops)
            disasm_str += " " + disasm::operand_to_str(op, write_mask_source);

        return result;
    };

    spv::Id op1_result = do_dual_op(op1.opcode, op1_srcs, op1.opr.dest, op1_write_mask, op1_info);
    if (op1_result == spv::NoResult)
        return false;
    disasm_str += " + ";
    spv::Id op2_result = do_dual_op(op2.opcode, op2_srcs, op2.opr.dest, op2_write_mask, op2_info);
    if (op2_result == spv::NoResult)
        return false;

    // Dual instructions are async. To simulate that we store after both instructions are completed.
    store(op1.opr.dest, op1_result, op1_write_mask);
    store(op2.opr.dest, op2_result, op2_write_mask);

    LOG_DISASM(disasm_str);

    return true;
}
