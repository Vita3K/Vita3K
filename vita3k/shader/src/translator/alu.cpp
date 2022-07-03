
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
#include <shader/translator.h>
#include <shader/types.h>
#include <util/log.h>

#include <optional>

namespace shader::usse {
bool USSETranslatorVisitor::vmad(
    ExtVecPredicate pred,
    Imm1 skipinv,
    Imm1 gpi1_swiz_ext,
    Imm1 opcode2,
    Imm1 dest_use_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    RepeatMode repeat_mode,
    Imm1 gpi0_abs,
    RepeatCount repeat_count,
    bool nosched,
    Imm4 write_mask,
    Imm1 src1_neg,
    Imm1 src1_abs,
    Imm1 gpi1_neg,
    Imm1 gpi1_abs,
    Imm1 gpi0_swiz_ext,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 gpi0_n,
    Imm6 dest_n,
    Imm4 gpi0_swiz,
    Imm4 gpi1_swiz,
    Imm2 gpi1_n,
    Imm1 gpi0_neg,
    Imm1 src1_swiz_ext,
    Imm4 src1_swiz,
    Imm6 src1_n) {
    // Is this VMAD3 or VMAD4, op2 = 0 => vec3
    int type = 2;

    if (opcode2 == 0) {
        type = 1;
    }

    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_bank_ext, true, 7, m_second_program);
    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank, dest_use_bank_ext, true, 7, m_second_program);

    // GPI0 and GPI1, setup!
    decoded_inst.opr.src0.bank = usse::RegisterBank::FPINTERNAL;
    decoded_inst.opr.src0.flags = RegisterFlags::GPI;
    decoded_inst.opr.src0.num = gpi0_n;

    decoded_inst.opr.src2.bank = usse::RegisterBank::FPINTERNAL;
    decoded_inst.opr.src2.flags = RegisterFlags::GPI;
    decoded_inst.opr.src2.num = gpi1_n;

    decoded_inst.opr.src0.type = DataType::F32;
    decoded_inst.opr.src1.type = DataType::F32;
    decoded_inst.opr.src2.type = DataType::F32;
    decoded_inst.opr.dest.type = DataType::F32;

    // Swizzleee
    if (type == 1) {
        decoded_inst.opr.dest.swizzle[3] = usse::SwizzleChannel::C_X;
    }

    decoded_inst.opr.src1.swizzle = decode_vec34_swizzle(src1_swiz, src1_swiz_ext, type);
    decoded_inst.opr.src0.swizzle = decode_vec34_swizzle(gpi0_swiz, gpi0_swiz_ext, type);
    decoded_inst.opr.src2.swizzle = decode_vec34_swizzle(gpi1_swiz, gpi1_swiz_ext, type);

    if (src1_abs) {
        decoded_inst.opr.src1.flags |= RegisterFlags::Absolute;
    }

    if (src1_neg) {
        decoded_inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    if (gpi0_abs) {
        decoded_inst.opr.src0.flags |= RegisterFlags::Absolute;
    }

    if (gpi0_neg) {
        decoded_inst.opr.src0.flags |= RegisterFlags::Negative;
    }

    if (gpi1_abs) {
        decoded_inst.opr.src2.flags |= RegisterFlags::Absolute;
    }

    if (gpi1_neg) {
        decoded_inst.opr.src2.flags |= RegisterFlags::Negative;
    }

    std::string disasm_str = fmt::format("{:016x}: {}{}", m_instr, disasm::e_predicate_str(ext_vec_predicate_to_ext(pred)), "VMAD");

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, repeat_mode);

    LOG_DISASM("{} {} {} {} {}", disasm_str, disasm::operand_to_str(decoded_inst.opr.dest, write_mask, dest_repeat_offset), disasm::operand_to_str(decoded_inst.opr.src0, write_mask, src0_repeat_offset), disasm::operand_to_str(decoded_inst.opr.src1, write_mask, src1_repeat_offset),
        disasm::operand_to_str(decoded_inst.opr.src2, write_mask, src2_repeat_offset));

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
    Opcode op = Opcode::VMAD;

    // If dat_fmt equals to 1, the type of instruction is F16
    if (dat_fmt) {
        op = Opcode::VF16MAD;
    }

    const DataType inst_dt = (dat_fmt) ? DataType::F16 : DataType::F32;

    // Decode mandantory info first
    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank, false, true, 7, m_second_program);
    decoded_inst.opr.src0 = decode_src0(decoded_inst.opr.src0, src0_n, src0_bank, false, true, 7, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_bank_ext, true, 7, m_second_program);
    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank, src2_bank_ext, true, 7, m_second_program);

    // Decode destination mask
    decoded_dest_mask = decode_write_mask(decoded_inst.opr.dest.bank, dest_mask, dat_fmt);

    // Fill in data type
    decoded_inst.opr.dest.type = inst_dt;
    decoded_inst.opr.src0.type = inst_dt;
    decoded_inst.opr.src1.type = inst_dt;
    decoded_inst.opr.src2.type = inst_dt;

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

    decoded_inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    decoded_inst.opr.src0.swizzle = tb_decode_src0_swiz[src0_swiz];
    decoded_inst.opr.src1.swizzle = tb_decode_src1_swiz[src1_swiz];
    decoded_inst.opr.src2.swizzle = tb_decode_src2_swiz[src2_swiz];

    // Decode modifiers
    if (src0_abs) {
        decoded_inst.opr.src0.flags |= RegisterFlags::Absolute;
    }

    decoded_inst.opr.src1.flags = decode_modifier(src1_mod);
    decoded_inst.opr.src2.flags = decode_modifier(src2_mod);

    // Log the instruction
    LOG_DISASM("{:016x}: {}{}2 {} {} {} {}", m_instr, disasm::e_predicate_str(static_cast<ExtPredicate>(pred)), disasm::opcode_str(op), disasm::operand_to_str(decoded_inst.opr.dest, dest_mask),
        disasm::operand_to_str(decoded_inst.opr.src0, decoded_dest_mask), disasm::operand_to_str(decoded_inst.opr.src1, dest_mask), disasm::operand_to_str(decoded_inst.opr.src2, dest_mask));

    return true;
}

bool USSETranslatorVisitor::vdp(
    ExtVecPredicate pred,
    Imm1 skipinv,
    bool clip_plane_enable,
    Imm1 opcode2,
    Imm1 dest_use_bank_ext,
    Imm1 end,
    Imm1 src1_bank_ext,
    RepeatMode repeat_mode,
    Imm1 gpi0_abs,
    RepeatCount repeat_count,
    bool nosched,
    Imm4 write_mask,
    Imm1 src1_neg,
    Imm1 src1_abs,
    Imm3 clip_plane_n,
    Imm2 dest_bank,
    Imm2 src1_bank,
    Imm2 gpi0_n,
    Imm6 dest_n,
    Imm4 gpi0_swiz,
    Imm3 src1_swiz_w,
    Imm3 src1_swiz_z,
    Imm3 src1_swiz_y,
    Imm3 src1_swiz_x,
    Imm6 src1_n) {
    // Is this VDP3 or VDP4, op2 = 0 => vec3
    int type = 2;
    usse::Imm4 src_mask = 0b1111;

    if (opcode2 == 0) {
        type = 1;
        src_mask = 0b0111;
    }

    // Double regs always true for src1, dest
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_bank_ext, true, 7, m_second_program);
    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank, dest_use_bank_ext, true, 7, m_second_program);

    decoded_inst.opr.src2.bank = usse::RegisterBank::FPINTERNAL;
    decoded_inst.opr.src2.flags = RegisterFlags::GPI;
    decoded_inst.opr.src2.type = DataType::F32;
    decoded_inst.opr.src2.num = gpi0_n;
    decoded_inst.opr.src2.swizzle = decode_vec34_swizzle(gpi0_swiz, false, type);

    // Decode first source swizzle
    const SwizzleChannel tb_swizz_dec[] = {
        SwizzleChannel::C_X,
        SwizzleChannel::C_Y,
        SwizzleChannel::C_Z,
        SwizzleChannel::C_W,
        SwizzleChannel::C_0,
        SwizzleChannel::C_1,
        SwizzleChannel::C_2,
        SwizzleChannel::C_H,
        SwizzleChannel::C_UNDEFINED
    };

    decoded_inst.opr.src1.swizzle[0] = tb_swizz_dec[src1_swiz_x];
    decoded_inst.opr.src1.swizzle[1] = tb_swizz_dec[src1_swiz_y];
    decoded_inst.opr.src1.swizzle[2] = tb_swizz_dec[src1_swiz_z];
    decoded_inst.opr.src1.swizzle[3] = tb_swizz_dec[src1_swiz_w];

    // Set modifiers
    if (src1_neg) {
        decoded_inst.opr.src1.flags |= RegisterFlags::Negative;
    }

    if (src1_abs) {
        decoded_inst.opr.src1.flags |= RegisterFlags::Absolute;
    }

    if (gpi0_abs) {
        decoded_inst.opr.src2.flags |= RegisterFlags::Absolute;
    }

    // Decoding done
    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, repeat_mode)

    LOG_DISASM("{:016x}: {}VDP {} {} {}", m_instr, disasm::e_predicate_str(ext_vec_predicate_to_ext(pred)), disasm::operand_to_str(decoded_inst.opr.dest, write_mask, 0),
        disasm::operand_to_str(decoded_inst.opr.src1, src_mask, src1_repeat_offset), disasm::operand_to_str(decoded_inst.opr.src2, src_mask, src2_repeat_offset));

    // Rotate write mask
    write_mask = (write_mask << 1) | (write_mask >> 3);
    END_REPEAT()

    return true;
}

bool USSETranslatorVisitor::v32nmad(
    ExtVecPredicate pred,
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
    decoded_inst.opcode = opcode;

    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank_sel, dest_bank_ext, true, 7, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank_sel, src1_bank_ext, true, 7, m_second_program);
    decoded_inst.opr.src1.flags = decode_modifier(src1_mod);
    decoded_inst.opr.src1.index = 1;

    decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank_sel, src2_bank_ext, true, 7, m_second_program);
    decoded_inst.opr.src2.index = 2;

    if (src2_mod == 1) {
        decoded_inst.opr.src2.flags = RegisterFlags::Absolute;
    }

    decoded_inst.opr.dest.type = is_32_bit ? DataType::F32 : DataType::F16;
    decoded_inst.opr.src1.type = decoded_inst.opr.dest.type;
    decoded_inst.opr.src2.type = decoded_inst.opr.dest.type;

    const auto src1_swizzle_enc = src1_swiz_0_6 | src1_swiz_7_8 << 7 | src1_swiz_9 << 9 | src1_swiz_10_11 << 10;
    decoded_inst.opr.src1.swizzle = decode_swizzle4(src1_swizzle_enc);
    decoded_inst.opr.src2.swizzle = decode_vec34_swizzle(src2_swiz, false, 2);

    // TODO: source modifiers
    decoded_source_mask = dest_mask;

    // Dot only produces one result. But use full components
    if ((decoded_inst.opcode == Opcode::VF16DP) || (decoded_inst.opcode == Opcode::VDP)) {
        decoded_source_mask = 0b1111;

        if (dest_mask_to_comp_count(dest_mask) != 1) {
            LOG_TRACE("VNMAD is VDP but has dest mask with more than 1 component active. Please report to developer to handle this!");
        }
    }

    ExtPredicate pred_translated = ext_vec_predicate_to_ext(pred);

    LOG_DISASM("{:016x}: {}{} {} {} {}", m_instr, disasm::e_predicate_str(pred_translated), disasm::opcode_str(opcode), disasm::operand_to_str(decoded_inst.opr.dest, dest_mask),
        disasm::operand_to_str(decoded_inst.opr.src1, decoded_source_mask), disasm::operand_to_str(decoded_inst.opr.src2, decoded_source_mask));

    return true;
}

bool USSETranslatorVisitor::v16nmad(
    ExtVecPredicate pred,
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
    // All of them needs to be doubled
    decoded_inst.opr.dest = decode_dest(decoded_inst.opr.dest, dest_n, dest_bank, dest_bank_ext, true, 8, m_second_program);
    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_bank_ext, true, 8, m_second_program);
    decoded_inst.opr.src1.flags = decode_modifier(src1_mod);

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
    decoded_inst.opr.src1.type = tb_decode_src_type[src_type];
    decoded_inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    decoded_inst.opr.dest.type = tb_decode_dest_type[dest_type];
    decoded_inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    decoded_inst.opcode = op;

    // TODO: Should we do this ?
    decoded_source_mask = 0;

    // Build the source mask. It should only be one component
    switch (src_comp) {
    case 0: {
        decoded_source_mask = 0b0001;
        break;
    }

    case 1: {
        decoded_source_mask = 0b0010;
        break;
    }

    case 2: {
        decoded_source_mask = 0b0100;
        break;
    }

    case 3: {
        decoded_source_mask = 0b1000;
        break;
    }

    default: break;
    }

    BEGIN_REPEAT(repeat_count)
    GET_REPEAT(decoded_inst, RepeatMode::SLMSI);

    LOG_DISASM("{:016x}: {}{} {} {}", m_instr, disasm::e_predicate_str(pred), disasm::opcode_str(op), disasm::operand_to_str(decoded_inst.opr.dest, write_mask, dest_repeat_offset),
        disasm::operand_to_str(decoded_inst.opr.src1, decoded_source_mask, src1_repeat_offset));

    END_REPEAT()

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

static std::optional<DualSrcLayout> get_dual_op1_src_layout(uint8_t count, Imm2 config) {
    switch (count) {
    case 1:
        switch (config) {
        case 0: return std::optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::NONE, DualSrcId::NONE });
        case 1: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::NONE, DualSrcId::NONE });
        case 2: return std::optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::NONE, DualSrcId::NONE });
        case 3: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL2, DualSrcId::NONE, DualSrcId::NONE });
        default:
            return {};
        }
    case 2:
        switch (config) {
        case 0: return std::optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::INTERNAL1, DualSrcId::NONE });
        case 1: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::NONE });
        case 2: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::NONE });
        default:
            return {};
        }
    case 3:
        switch (config) {
        case 0: return std::optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::INTERNAL1, DualSrcId::INTERAL2 });
        case 1: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::INTERAL2 });
        case 2: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::UNIFIED });
        case 3: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::INTERAL2 });
        default:
            return {};
        }
    default:
        return {};
    }

    return {};
}

// Dual op2 layout depends on op1's src layout too...
static std::optional<DualSrcLayout> get_dual_op2_src_layout(uint8_t op1_count, uint8_t op2_count, Imm2 src_config) {
    switch (op1_count) {
    case 1:
        switch (op2_count) {
        case 1:
            switch (src_config) {
            case 0: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::NONE, DualSrcId::NONE });
            case 1:
            case 2:
            case 3:
                return std::optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::NONE, DualSrcId::NONE });
            default: return {};
            }
        case 2:
            switch (src_config) {
            case 0: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::NONE });
            case 1: return std::optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::INTERNAL1, DualSrcId::NONE });
            case 2: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::NONE });
            case 3: return std::optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::UNIFIED, DualSrcId::NONE });
            default: return {};
            }
        case 3:
            switch (src_config) {
            case 0: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERNAL1, DualSrcId::INTERAL2 });
            case 1: return std::optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::INTERNAL1, DualSrcId::INTERAL2 });
            case 2: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::INTERAL2 });
            case 3: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::UNIFIED, DualSrcId::INTERNAL1 });
            default: return {};
            }
        default: return {};
        }
    case 2:
        switch (op2_count) {
        case 1:
            switch (src_config) {
            case 0: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::NONE, DualSrcId::NONE });
            case 1: return std::optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::NONE, DualSrcId::NONE });
            case 2: return std::optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::NONE, DualSrcId::NONE });
            default: return {};
            }
        case 2:
            switch (src_config) {
            case 0: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::INTERAL2, DualSrcId::NONE });
            case 1: return std::optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::INTERAL2, DualSrcId::NONE });
            case 2: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL2, DualSrcId::UNIFIED, DualSrcId::NONE });
            default: return {};
            }
        default: return {};
        }
    case 3:
        switch (op2_count) {
        case 1:
            switch (src_config) {
            case 0: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL0, DualSrcId::NONE, DualSrcId::NONE });
            case 1: return std::optional<DualSrcLayout>({ DualSrcId::INTERNAL1, DualSrcId::NONE, DualSrcId::NONE });
            case 2: return std::optional<DualSrcLayout>({ DualSrcId::INTERAL2, DualSrcId::NONE, DualSrcId::NONE });
            case 3: return std::optional<DualSrcLayout>({ DualSrcId::UNIFIED, DualSrcId::NONE, DualSrcId::NONE });
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

    decoded_inst.opcode = op1_codes[(!comp_count_type && dual_op1_ext_vec3_or_has_w_vec4) << 3u | dual_op1];
    decoded_inst_2.opcode = op2_codes[dual_op2_ext << 3u | dual_op2];

    const auto op1_info = op_info.at(decoded_inst.opcode);
    const auto op2_info = op_info.at(decoded_inst_2.opcode);

    // Unified is only part of instruction that can reference any bank. Others are internal.
    Operand unified_dest;
    Operand internal_dest;

    bool unified_is_vector = false;
    if ((prim_ustore && op1_info.vector_store) || (!prim_ustore && op2_info.vector_store)) {
        unified_is_vector = true;
    }

    unified_dest = decode_dest(unified_dest, prim_dest_num, prim_dest_bank, false, unified_is_vector, unified_is_vector ? 8 : 7, m_second_program);
    unified_dest.type = (type_f16 ? DataType::F16 : DataType::F32);
    unified_dest.index = 3;

    internal_dest.bank = RegisterBank::FPINTERNAL;
    internal_dest.num = prim_dest_num_gpi_case;
    internal_dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    internal_dest.index = 3;

    // op1 is primary instruction. op2 is secondary.
    decoded_inst.opr.dest = prim_ustore ? unified_dest : internal_dest;
    decoded_inst_2.opr.dest = prim_ustore ? internal_dest : unified_dest;

    const std::optional<DualSrcLayout> op1_layout = get_dual_op1_src_layout(op1_info.src_count, src_config);
    const std::optional<DualSrcLayout> op2_layout = get_dual_op2_src_layout(op1_info.src_count, op2_info.src_count, src_config);

    if (!op1_layout) {
        LOG_ERROR("Missing dual for op1 layout.");
        return false;
    }

    if (!op2_layout) {
        LOG_ERROR("Missing dual for op2 layout.");
        return false;
    }

    Imm4 fixed_write_mask = write_mask_non_gpi | ((comp_count_type && dual_op1_ext_vec3_or_has_w_vec4) << 3u);
    const size_t op1_src_count = op1_info.src_count;

    decoded_dest_mask = prim_ustore ? get_dual_op_write_mask(op1_info, decoded_inst.opr.dest.bank == RegisterBank::FPINTERNAL) : fixed_write_mask;
    decoded_dest_mask_2 = prim_ustore ? fixed_write_mask : get_dual_op_write_mask(op2_info, decoded_inst_2.opr.dest.bank == RegisterBank::FPINTERNAL);

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
                    op1_src_count >= 2 ? false : gpi2_slot_num_bit_1, comp_count_type);
                if ((op1_src_count <= 2) && gpi2_slot_num_bit_0_or_unified_store_abs)
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
                op.num = op1_src_count >= 2 ? (gpi2_slot_num_bit_1 << 1u | gpi2_slot_num_bit_0_or_unified_store_abs) : 2;
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

    decoded_source_mask = op1_info.vector_load ? decoded_dest_mask : 0b0001;
    if (op1_info.vector_load && ((decoded_inst.opcode == Opcode::VDP) || (decoded_inst.opcode == Opcode::VSSQ))) {
        decoded_source_mask = (0b1111u >> (uint32_t)!comp_count_type);
    }

    decoded_source_mask_2 = op2_info.vector_load ? decoded_dest_mask_2 : 0b0001;
    if (op2_info.vector_load && ((decoded_inst_2.opcode == Opcode::VDP) || (decoded_inst_2.opcode == Opcode::VSSQ))) {
        decoded_source_mask_2 = (0b1111u >> (uint32_t)!comp_count_type);
    }

    std::string disasm_str = fmt::format("{:016x}: ", m_instr);
    disasm_str += fmt::format("{} {}", disasm::opcode_str(decoded_inst.opcode), disasm::operand_to_str(decoded_inst.opr.dest, decoded_dest_mask));

    for (Operand &op : op1_srcs)
        disasm_str += " " + disasm::operand_to_str(op, decoded_source_mask);

    disasm_str += " + ";
    disasm_str += fmt::format("{} {}", disasm::opcode_str(decoded_inst_2.opcode), disasm::operand_to_str(decoded_inst_2.opr.dest, decoded_dest_mask_2));

    for (Operand &op : op2_srcs)
        disasm_str += " " + disasm::operand_to_str(op, decoded_source_mask_2);

    if (op1_srcs.size() > 2) {
        decoded_inst.opr.src2 = op1_srcs[2];
    }

    if (op1_srcs.size() > 1) {
        decoded_inst.opr.src1 = op1_srcs[1];
    }

    decoded_inst.opr.src0 = op1_srcs[0];

    if (op2_srcs.size() > 2) {
        decoded_inst_2.opr.src2 = op2_srcs[2];
    }

    if (op2_srcs.size() > 1) {
        decoded_inst_2.opr.src1 = op2_srcs[1];
    }

    decoded_inst_2.opr.src0 = op2_srcs[0];
    LOG_DISASM("{}", disasm_str);

    return true;
}
} // namespace shader::usse