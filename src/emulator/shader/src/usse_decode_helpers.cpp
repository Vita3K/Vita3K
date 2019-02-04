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

#include <shader/usse_decode_helpers.h>

#include <util/log.h>

using namespace USSE;

namespace shader {
namespace usse {

//
// Decoder helpers
//

/**
 * \brief Changes TEMP registers to FPINTERNAL if certain conditions are met
 */
static void check_reg_internal(Operand &inout_reg, bool is_double_regs) {
    const auto max_reg_num = is_double_regs ? 128 : 64;
    const auto temp_reg_limit = max_reg_num - (is_double_regs ? 8 : 4);

    if (inout_reg.bank == RegisterBank::TEMP && inout_reg.num >= temp_reg_limit) {
        inout_reg.num -= temp_reg_limit;
        if (is_double_regs)
            inout_reg.num >>= 1;
        inout_reg.bank = RegisterBank::FPINTERNAL;
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

// Register/Operand decoding

static void finalize_register(Operand &reg, bool is_double_regs) {
    check_reg_internal(reg, is_double_regs);
}

Operand decode_dest(Imm6 dest_n, Imm2 dest_bank, bool bank_ext, bool is_double_regs) {
    Operand dest{};
    dest.num = dest_n;
    dest.bank = decode_dest_bank(dest_bank, bank_ext);

    if (is_double_regs)
        double_reg(dest.num, dest.bank);

    finalize_register(dest, is_double_regs);
    return dest;
}

Operand decode_src12(Imm6 src_n, Imm2 src_bank_sel, Imm1 src_bank_ext, bool is_double_regs) {
    Operand src{};
    src.num = src_n;
    src.bank = decode_src12_bank(src_bank_sel, src_bank_ext);

    if (is_double_regs)
        double_reg(src.num, src.bank);

    finalize_register(src, is_double_regs);
    return src;
}

} // namespace usse
} // namespace shader