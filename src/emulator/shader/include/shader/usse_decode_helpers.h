#pragma once

#include <shader/types.h>

namespace shader {
namespace usse {

//
// Decoder helpers
//

USSE::Swizzle4 decode_swizzle4(uint32_t encoded_swizzle);
USSE::Swizzle4 decode_vec34_swizzle(USSE::Imm4 swizz, const bool swizz_extended, const int type);

// Register/Operand decoding

USSE::Operand decode_dest(USSE::Imm6 dest_n, USSE::Imm2 dest_bank, bool bank_ext, bool is_double_regs);
USSE::Operand decode_src12(USSE::Imm6 src_n, USSE::Imm2 src_bank_sel, USSE::Imm1 src_bank_ext, bool is_double_regs);

} // namespace usse
} // namespace shader
