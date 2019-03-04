// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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
#include <shader/usse_types.h>

#include <util/log.h>

namespace shader::usse {

//
// Decoder helpers
//

/**
 * \brief Changes TEMP registers to FPINTERNAL if certain conditions are met
 */
static void check_reg_internal(Operand &inout_reg, bool is_double_regs, uint8_t reg_bits) {
    const auto temps = is_double_regs ? 8 : 4;
    const auto max_reg_num = 1 << reg_bits;
    const auto temp_reg_limit = max_reg_num - temps;

    if (inout_reg.bank == RegisterBank::TEMP && inout_reg.num >= temp_reg_limit) {
        inout_reg.num -= temp_reg_limit;
        if (is_double_regs)
            inout_reg.num >>= 1;
        inout_reg.bank = RegisterBank::FPINTERNAL;
    }
}

/**
 * \brief Changes SPECIAL registers to GLOBAL or FPCONSTANT
 */
static void fixup_reg_special(Operand &inout_reg) {
    const auto SPECIAL_GLOBAL_FLAG = 0x40;

    if (inout_reg.num & SPECIAL_GLOBAL_FLAG) {
        inout_reg.num &= ~SPECIAL_GLOBAL_FLAG;
        inout_reg.bank = RegisterBank::GLOBAL;
    } else {
        inout_reg.bank = RegisterBank::FPCONSTANT;
    }
}

/**
 * \brief Doubles 'reg' register if necessary
 */
static void double_reg(Imm6 &reg, RegisterBank reg_bank) {
    if (reg_bank != RegisterBank::SPECIAL && reg_bank != RegisterBank::IMMEDIATE)
        reg <<= 1;
}

// Operand bank decoding

static RegisterBank decode_dest_bank(Imm2 dest_bank, bool bank_ext) {
    if (dest_bank == 3)
        return RegisterBank::INDEXED;
    else
        // TODO: Index stuff
        if (bank_ext)
        switch (dest_bank) {
        case 0: return RegisterBank::SECATTR;
        case 1: return RegisterBank::SPECIAL;
        case 2: return RegisterBank::INDEX;
        case 3: return RegisterBank::FPINTERNAL;
        }
    else
        switch (dest_bank) {
        case 0: return RegisterBank::TEMP;
        case 1: return RegisterBank::OUTPUT;
        case 2: return RegisterBank::PRIMATTR;
        }
    LOG_ERROR("Invalid dest_bank");
    return RegisterBank::INVALID;
}

static RegisterBank decode_src0_bank(Imm2 src0_bank, Imm1 bank_ext) {
    if (bank_ext)
        switch (src0_bank) {
        case 0: return RegisterBank::OUTPUT;
        case 1: return RegisterBank::SECATTR;
        }
    else
        switch (src0_bank) {
        case 0: return RegisterBank::TEMP;
        case 1: return RegisterBank::PRIMATTR;
        }
    LOG_ERROR("Invalid src0_bank");
    return RegisterBank::INVALID;
}

static RegisterBank decode_src12_bank(Imm2 src12_bank, Imm1 bank_ext) {
    // TODO: Index stuff
    if (bank_ext)
        switch (src12_bank) {
        case 1: return RegisterBank::SPECIAL;
        case 2: return RegisterBank::IMMEDIATE;
        }
    else
        switch (src12_bank) {
        case 0: return RegisterBank::TEMP;
        case 1: return RegisterBank::OUTPUT;
        case 2: return RegisterBank::PRIMATTR;
        case 3: return RegisterBank::SECATTR;
        }
    LOG_ERROR("Invalid src12_bank");
    return RegisterBank::INVALID;
}

// Swizzle decoding

Swizzle4 decode_swizzle4(uint32_t encoded_swizzle) {
    return {
        (SwizzleChannel)((encoded_swizzle & 0b000'000'000'111) >> 0),
        (SwizzleChannel)((encoded_swizzle & 0b000'000'111'000) >> 3),
        (SwizzleChannel)((encoded_swizzle & 0b000'111'000'000) >> 6),
        (SwizzleChannel)((encoded_swizzle & 0b111'000'000'000) >> 9),
    };
}

Swizzle4 decode_vec34_swizzle(Imm4 swizz, const bool swizz_extended, const int type) {
    static Swizzle4 swizz_v4_std[] = {
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
        SWIZZLE_CHANNEL_4(X, Y, Z, 1)
    };

    static Swizzle4 swizz_v4_ext[] = {
        SWIZZLE_CHANNEL_4(Y, Z, X, W),
        SWIZZLE_CHANNEL_4(Z, W, X, Y),
        SWIZZLE_CHANNEL_4(X, Z, W, Y),
        SWIZZLE_CHANNEL_4(Y, Y, W, W),
        SWIZZLE_CHANNEL_4(W, Y, Z, W),
        SWIZZLE_CHANNEL_4(W, Z, W, Z),
        SWIZZLE_CHANNEL_4(X, Y, Z, X),
        SWIZZLE_CHANNEL_4(Z, Z, W, W),
        SWIZZLE_CHANNEL_4(X, W, Z, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, Z),
        SWIZZLE_CHANNEL_4(X, Z, Y, W),
        SWIZZLE_CHANNEL_4(X, X, X, Y),
        SWIZZLE_CHANNEL_4(Z, Y, X, W),
        SWIZZLE_CHANNEL_4(Y, Y, Z, Z),
        SWIZZLE_CHANNEL_4(Z, Z, Z, Y)
    };

    static Swizzle3 swizz_v3_std[] = {
        SWIZZLE_CHANNEL_3(X, X, X),
        SWIZZLE_CHANNEL_3(Y, Y, Y),
        SWIZZLE_CHANNEL_3(Z, Z, Z),
        SWIZZLE_CHANNEL_3(W, W, W),
        SWIZZLE_CHANNEL_3(X, Y, Z),
        SWIZZLE_CHANNEL_3(Y, Z, W),
        SWIZZLE_CHANNEL_3(X, X, Y),
        SWIZZLE_CHANNEL_3(X, Y, X),

        SWIZZLE_CHANNEL_3(Y, Y, X),
        SWIZZLE_CHANNEL_3(Y, Y, Z),
        SWIZZLE_CHANNEL_3(Z, X, Y),
        SWIZZLE_CHANNEL_3(X, Z, Y),
        SWIZZLE_CHANNEL_3(Y, Z, X),
        SWIZZLE_CHANNEL_3(Z, Y, X),
        SWIZZLE_CHANNEL_3(Z, Z, Y),
        SWIZZLE_CHANNEL_3(X, Y, 1)
    };

    static Swizzle3 swizz_v3_ext[] = {
        SWIZZLE_CHANNEL_3(X, Y, Y),
        SWIZZLE_CHANNEL_3(Y, X, Y),
        SWIZZLE_CHANNEL_3(X, X, Z),
        SWIZZLE_CHANNEL_3(Y, X, X),
        SWIZZLE_CHANNEL_3(X, Y, 0),
        SWIZZLE_CHANNEL_3(X, 1, 0),
        SWIZZLE_CHANNEL_3(0, 0, 0),
        SWIZZLE_CHANNEL_3(1, 1, 1),
        SWIZZLE_CHANNEL_3(H, H, H),
        SWIZZLE_CHANNEL_3(2, 2, 2),
        SWIZZLE_CHANNEL_3(X, 0, 0),
        SWIZZLE_CHANNEL_3_UNDEFINED,
        SWIZZLE_CHANNEL_3_UNDEFINED,
        SWIZZLE_CHANNEL_3_UNDEFINED,
        SWIZZLE_CHANNEL_3_UNDEFINED
    };

    static Swizzle4 swizz_scalar_std[] = {
        SWIZZLE_CHANNEL_4(X, X, X, X),
        SWIZZLE_CHANNEL_4(Y, Y, Y, Y),
        SWIZZLE_CHANNEL_4(Z, Z, Z, Z),
        SWIZZLE_CHANNEL_4(W, W, W, W),
        SWIZZLE_CHANNEL_4(0, 0, 0, 0),
        SWIZZLE_CHANNEL_4(1, 1, 1, 1),
        SWIZZLE_CHANNEL_4(2, 2, 2, 2),
        SWIZZLE_CHANNEL_4(H, H, H, H),
        SWIZZLE_CHANNEL_4_UNDEFINED,
        SWIZZLE_CHANNEL_4_UNDEFINED,
        SWIZZLE_CHANNEL_4_UNDEFINED,
        SWIZZLE_CHANNEL_4_UNDEFINED,
        SWIZZLE_CHANNEL_4_UNDEFINED,
        SWIZZLE_CHANNEL_4_UNDEFINED,
        SWIZZLE_CHANNEL_4_UNDEFINED,
        SWIZZLE_CHANNEL_4_UNDEFINED,
    };

    if (type == 0) {
        if (swizz_extended) {
            return SWIZZLE_CHANNEL_4_UNDEFINED;
        } else {
            return swizz_scalar_std[swizz];
        }
    }

    // vec3
    if (type == 1) {
        if (swizz_extended) {
            return to_swizzle4(swizz_v3_ext[swizz]);
        } else {
            return to_swizzle4(swizz_v3_std[swizz]);
        }
    }

    // vec4
    if (type == 2) {
        if (swizz_extended) {
            return swizz_v4_ext[swizz];
        } else {
            return swizz_v4_std[swizz];
        }
    }

    // this is unreachable
    return SWIZZLE_CHANNEL_4_UNDEFINED;
}

// Register/Operand decoding

static void finalize_register(Operand &reg, bool is_double_regs, uint8_t reg_bits) {
    check_reg_internal(reg, is_double_regs, reg_bits);

    if (reg.bank == RegisterBank::SPECIAL)
        fixup_reg_special(reg);
}

Operand decode_dest(Imm6 dest_n, Imm2 dest_bank, bool bank_ext, bool is_double_regs, uint8_t reg_bits) {
    Operand dest{};
    dest.num = dest_n;
    dest.bank = decode_dest_bank(dest_bank, bank_ext);
    dest.swizzle = SWIZZLE_CHANNEL_4_DEFAULT;

    if (is_double_regs)
        double_reg(dest.num, dest.bank);

    finalize_register(dest, is_double_regs, reg_bits);
    return dest;
}

Operand decode_src12(Imm6 src_n, Imm2 src_bank_sel, Imm1 src_bank_ext, bool is_double_regs, uint8_t reg_bits) {
    Operand src{};
    src.num = src_n;
    src.bank = decode_src12_bank(src_bank_sel, src_bank_ext);

    if (is_double_regs)
        double_reg(src.num, src.bank);

    finalize_register(src, is_double_regs, reg_bits);
    return src;
}

} // namespace shader::usse