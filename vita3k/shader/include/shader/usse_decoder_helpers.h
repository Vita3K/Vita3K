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

#pragma once

#include <shader/usse_types.h>

namespace shader {
namespace usse {

//
// Decoder helpers
//

usse::Swizzle4 decode_swizzle4(uint32_t encoded_swizzle);
usse::Swizzle4 decode_vec34_swizzle(usse::Imm4 swizz, const bool swizz_extended, const int type);
usse::Swizzle4 decode_dual_swizzle(Imm4 swizz, const bool extended, const bool vec4);
usse::Imm4 decode_write_mask(RegisterBank dest_bank, usse::Imm4 write_mask, const bool f16);
usse::RegisterFlags decode_modifier(usse::Imm2 mod);

// Register/Operand decoding

usse::Operand &decode_dest(usse::Operand &dest, usse::Imm6 dest_n, usse::Imm2 dest_bank, bool bank_ext, bool is_double_regs, uint8_t reg_bits,
    bool is_second_program);
usse::Operand &decode_src12(usse::Operand &src, usse::Imm6 src_n, usse::Imm2 src_bank_sel, usse::Imm1 src_bank_ext, bool is_double_regs, uint8_t reg_bits,
    bool is_second_program);
usse::Operand &decode_src0(usse::Operand &src, usse::Imm6 src_n, usse::Imm1 src_bank_sel, usse::Imm1 src_bank_ext, bool is_double_regs, uint8_t reg_bits,
    bool is_second_program);

} // namespace usse
} // namespace shader
