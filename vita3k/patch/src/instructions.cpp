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

#include "patch/instructions.h"

#include <util/log.h>

uint32_t nop(std::vector<uint32_t> &args) {
    return 0;
}

// args[0] - destination register Rd (0–7 only)
// args[1] - immediate value (8-bit, 0–255)
uint32_t t1_mov(std::vector<uint32_t> &args) {
    uint8_t b0 = args[1]; // imm8 (lower byte)
    uint8_t b1 = 0b00100000 | (args[0] << 8); // opcode + Rd (upper byte)

    return (b1 << 8) | b0;
}

// args[0] = sf (0 = MOV, 1 = MOVS)
// args[1] = Rd (0–15)
// args[2] = imm (Only compatible with 0–255)
uint32_t a1_mov(std::vector<uint32_t> &args) {
    uint8_t sf = args[0] & 1;
    uint8_t rd = args[1] & 0xF;
    uint8_t imm = args[2] & 0xFF;

    uint32_t instr = 0;

    instr |= 0b1110u << 28; // cond = AL
    instr |= 0b0011101u << 21; // opcode = MOV (0011101)
    instr |= sf << 20; // S bit
    instr |= rd << 12; // Rd
    instr |= imm; // imm8 (rotate = 0)

    return instr;
}
