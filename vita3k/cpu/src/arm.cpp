// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <cpu/arm.h>

// Encode code taken from https://github.com/yifanlu/UVLoader/blob/master/resolve.c

namespace arm {

uint32_t encode_arm_inst(Instruction type, uint32_t immed, uint16_t reg) {
    switch (type) {
    case Instruction::MOVW:
        // 1110 0011 0000 XXXX YYYY XXXXXXXXXXXX
        // where X is the immediate and Y is the register
        return ARM_MOVW | ((immed & 0xF000) << 4) | (immed & 0xFFF) | (reg << 12);
    case Instruction::MOVT:
        // 1110 0011 0100 XXXX YYYY XXXXXXXXXXXX
        // where X is the immediate and Y is the register
        return ARM_MOVT | ((immed & 0xF000) << 4) | (immed & 0xFFF) | (reg << 12);
    case Instruction::SYSCALL:
        // Syscall does not have any immediate value, the number should
        // already be in R12
        return ARM_SVC;
    case Instruction::BRANCH:
        // 1110 0001 0010 111111111111 0001 YYYY
        return ARM_BX | reg;
    case Instruction::BLX:
        return ARM_BLX_IMM | ((immed & 0x80000000) >> 8) | (immed & 0x7fffff);
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
    return THUMB_BLX_IMM | (S << 26) | (immhi << 16) | (J1 << 13) | (J2 << 11) | (immlo << 1);
}

uint32_t encode_thumb_inst(Instruction type, uint32_t immed, uint16_t reg) {
    switch (type) {
    case Instruction::MOVW:
        return THUMB_MOVW | ((immed & 0x800) << 15) | ((immed & 0xf000) << 4) | ((immed & 0x700) << 4) | (reg << 8) | (immed & 0xff);
    case Instruction::MOVT:
        return THUMB_MOVT | ((immed & 0x800) << 15) | ((immed & 0xf000) << 4) | ((immed & 0x700) << 4) | (reg << 8) | (immed & 0xff);
    case Instruction::BRANCH:
        return ((THUMB_BX | (reg << 3)) << 16) | THUMB_NOP;
    case Instruction::BLX:
        return encode_thumb_blx(immed);
    default:
        return 0;
    }
}

} // namespace arm
