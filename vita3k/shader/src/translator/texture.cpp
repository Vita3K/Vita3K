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

namespace shader::usse {
bool USSETranslatorVisitor::smp(
    ExtPredicate pred,
    Imm1 skipinv,
    Imm1 nosched,
    Imm1 syncstart,
    Imm1 minpack,
    Imm1 src0_ext,
    Imm1 src1_ext,
    Imm1 src2_ext,
    Imm2 fconv_type,
    Imm2 mask_count,
    Imm2 dim,
    Imm2 lod_mode,
    bool dest_use_pa,
    Imm2 sb_mode,
    Imm2 src0_type,
    Imm1 src0_bank,
    Imm2 drc_sel,
    Imm2 src1_bank,
    Imm2 src2_bank,
    Imm7 dest_n,
    Imm7 src0_n,
    Imm7 src1_n,
    Imm7 src2_n) {
    constexpr DataType tb_dest_fmt[] = {
        DataType::F32,
        DataType::UNK,
        DataType::F16,
        DataType::F32
    };

    // Decode dest
    decoded_inst.opr.dest.bank = (dest_use_pa) ? RegisterBank::PRIMATTR : RegisterBank::TEMP;
    decoded_inst.opr.dest.num = dest_n;
    decoded_inst.opr.dest.type = tb_dest_fmt[fconv_type];

    // Decode src0
    decoded_inst.opr.src0 = decode_src0(decoded_inst.opr.src0, src0_n, src0_bank, src0_ext, true, 8, m_second_program);
    decoded_inst.opr.src0.type = (src0_type == 0) ? DataType::F32 : ((src0_type == 1) ? DataType::F16 : DataType::C10);

    decoded_inst.opr.src1 = decode_src12(decoded_inst.opr.src1, src1_n, src1_bank, src1_ext, true, 8, m_second_program);

    decoded_inst.opr.src1.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    decoded_inst.opr.src0.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;
    decoded_inst.opr.dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    // Base 0, turn it to base 1
    dim += 1;

    std::uint32_t coord_mask = 0b0011;
    if (dim == 3) {
        coord_mask = 0b0111;
    } else if (dim == 1) {
        coord_mask = 0b0001;
    }

    if (lod_mode != 0) {
        decoded_inst.opr.src2 = decode_src12(decoded_inst.opr.src2, src2_n, src2_bank, src2_ext, true, 8, m_second_program);
        decoded_inst.opr.src2.type = decoded_inst.opr.src0.type;
    }

    LOG_DISASM("{:016x}: {}SMP{}d.{}.{} {} {} {} {}", m_instr, disasm::e_predicate_str(pred), dim, disasm::data_type_str(decoded_inst.opr.dest.type), disasm::data_type_str(decoded_inst.opr.src0.type),
        disasm::operand_to_str(decoded_inst.opr.dest, 0b0001), disasm::operand_to_str(decoded_inst.opr.src0, coord_mask), disasm::operand_to_str(decoded_inst.opr.src1, 0b0000), (lod_mode == 0) ? "" : disasm::operand_to_str(decoded_inst.opr.src2, 0b0001));

    return true;
}
} // namespace shader::usse