// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <util/arm.h>

// Encode code taken from https://github.com/yifanlu/UVLoader/blob/master/resolve.c

uint32_t encode_arm_inst(uint8_t type, uint32_t immed, uint16_t reg) {
    switch (type) {
    case INSTRUCTION_MOVW:
        // 1110 0011 0000 XXXX YYYY XXXXXXXXXXXX
        // where X is the immediate and Y is the register
        // Upper bits == 0xE30
        return (0xE30u << 20) | ((immed & 0xF000) << 4) | (immed & 0xFFF) | (reg << 12);
    case INSTRUCTION_MOVT:
        // 1110 0011 0100 XXXX YYYY XXXXXXXXXXXX
        // where X is the immediate and Y is the register
        // Upper bits == 0xE34
        return (0xE34u << 20) | ((immed & 0xF000) << 4) | (immed & 0xFFF) | (reg << 12);
    case INSTRUCTION_SYSCALL:
        // Syscall does not have any immediate value, the number should
        // already be in R12
        return 0xEF000000u;
    case INSTRUCTION_BRANCH:
        // 1110 0001 0010 111111111111 0001 YYYY
        // BX Rn has 0xE12FFF1 as top bytes
        return (0xE12FFF1u << 4) | reg;
    case INSTRUCTION_BLX:
        return (0x7Du << 25) | ((immed & 0x80000000) >> 8) | (immed & 0x7fffff);
    case INSTRUCTION_UNKNOWN:
    default:
        return 0;
    }
}

inline static uint32_t encode_thumb_blx(uint32_t immed) {
    const uint32_t S = (immed & 0x80000000) >> 31;
    const uint32_t I1 = (immed & 0x800000) >> 22;
    const uint32_t I2 = (immed & 0x400000) >> 21;
    const uint32_t immhi = (immed & 0x3ff000) >> 11;
    const uint32_t immlo = (immed & 0xffc) >> 2;
    const uint32_t J1 = ~I1 ^ S;
    const uint32_t J2 = ~I2 ^ S;
    return (0x1Eu << 27) | (S << 26) | (immhi << 16) | (0x3u << 14) | (J1 << 13) | (J2 << 11) | (immlo << 1);
}

uint32_t encode_thumb_inst(uint8_t type, uint32_t immed, uint16_t reg) {
    switch (type) {
    case INSTRUCTION_MOVW:
        return (0x1Eu << 27) | ((immed & 0x800) << 15) | (0x24u << 20) | ((immed & 0xf000) << 4) | ((immed & 0x700) << 4) | (reg << 8) | (immed & 0xff);
    case INSTRUCTION_MOVT:
        return (0x1Eu << 27) | ((immed & 0x800) << 15) | (0x2Cu << 20) | ((immed & 0xf000) << 4) | ((immed & 0x700) << 4) | (reg << 8) | (immed & 0xff);
    case INSTRUCTION_BRANCH:
        return (((0x8Eu << 7) | (reg << 3)) << 16) | 0xBF00u;
    case INSTRUCTION_BLX:
        return encode_thumb_blx(immed);
    case INSTRUCTION_UNKNOWN:
    default:
        return 0;
    }
}
