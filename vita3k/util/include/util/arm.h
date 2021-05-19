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

#include <cstdint>

#pragma once

#define INSTRUCTION_UNKNOWN 0 ///< Unknown/unsupported instruction
#define INSTRUCTION_MOVW 1 ///< MOVW Rd, \#imm instruction
#define INSTRUCTION_MOVT 2 ///< MOVT Rd, \#imm instruction
#define INSTRUCTION_SYSCALL 3 ///< SVC \#imm instruction
#define INSTRUCTION_BRANCH 4 ///< BX Rn instruction
#define INSTRUCTION_BLX 5 ///< BLX imm instruction

uint32_t encode_arm_inst(uint8_t type, uint32_t immed, uint16_t reg);
// SVC not implemented for thumb
uint32_t encode_thumb_inst(uint8_t type, uint32_t immed, uint16_t reg);