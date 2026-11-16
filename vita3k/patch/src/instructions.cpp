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

#include <cpu/arm.h>

#include <util/log.h>

uint32_t nop(std::vector<uint32_t> &args) {
    return arm::THUMB_NOP;
}

// args[0] - destination register Rd (0-7 only)
// args[1] - immediate value (8-bit, 0-255)
uint32_t t1_mov(std::vector<uint32_t> &args) {
    uint8_t Rd = args[0] & 0x7; // register index (3-bit)
    uint8_t imm = args[1] & 0xFF; // immediate value (8-bit)

    return arm::THUMB_MOV_IMM | (Rd << 8) | imm;
}

// args[0] = sf (0 = MOV, 1 = MOVS)
// args[1] = Rd (0-15)
// args[2] = imm (only compatible with 0-255)
uint32_t a1_mov(std::vector<uint32_t> &args) {
    uint8_t sf = args[0] & 1;
    uint8_t rd = args[1] & 0xF;
    uint8_t imm = args[2] & 0xFF;

    return arm::ARM_MOV_IMM | (sf << 20) | (rd << 12) | imm;
}
