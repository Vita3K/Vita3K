#pragma once

#include <shader/usse_types.h>

namespace shader {
namespace usse {

//
// Decoder helpers
//

usse::Swizzle4 decode_swizzle4(uint32_t encoded_swizzle);
usse::Swizzle4 decode_vec34_swizzle(usse::Imm4 swizz, const bool swizz_extended, const int type);
usse::Imm4 decode_write_mask(usse::Imm4 write_mask, const bool f16);
usse::RegisterFlags decode_modifier(usse::Imm2 mod);

// Register/Operand decoding

usse::Operand decode_dest(usse::Imm6 dest_n, usse::Imm2 dest_bank, bool bank_ext, bool is_double_regs, uint8_t reg_bits,
    bool is_second_program);
usse::Operand decode_src12(usse::Imm6 src_n, usse::Imm2 src_bank_sel, usse::Imm1 src_bank_ext, bool is_double_regs, uint8_t reg_bits,
    bool is_second_program);

} // namespace usse
} // namespace shader
