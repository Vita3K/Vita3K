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

#pragma once

#include <cstdint>

// Names from the Arm Architecture Reference Manual (ARMv7-A) and the AAPCS
namespace arm {

// Core register numbers
inline constexpr uint32_t REG_IP = 12;
inline constexpr uint32_t REG_SP = 13;
inline constexpr uint32_t REG_LR = 14;
inline constexpr uint32_t REG_PC = 15;
inline constexpr uint32_t CORE_REG_COUNT = 16;

// Advanced SIMD and VFPv3-D32 extension registers
inline constexpr uint32_t D_REG_COUNT = 32;
inline constexpr uint32_t Q_REG_COUNT = 16;
inline constexpr uint32_t WORDS_PER_D_REG = 2;
inline constexpr uint32_t WORDS_PER_Q_REG = 4;
inline constexpr uint32_t EXT_REG_WORDS = D_REG_COUNT * WORDS_PER_D_REG;

// CPSR fields
inline constexpr uint32_t CPSR_M = 0x0000001FU;
inline constexpr uint32_t CPSR_T = 1U << 5;
inline constexpr uint32_t CPSR_F = 1U << 6;
inline constexpr uint32_t CPSR_I = 1U << 7;
inline constexpr uint32_t CPSR_A = 1U << 8;
inline constexpr uint32_t CPSR_RESERVED_23_20 = 0x00F00000U;
inline constexpr uint32_t CPSR_IT_1_0 = 3U << 25;
inline constexpr uint32_t CPSR_MODE_USR = 0x10U;

// FPSCR fields
inline constexpr uint32_t FPSCR_FZ = 1U << 24;
inline constexpr uint32_t FPSCR_DN = 1U << 25;

// Bit 0 of an interworking branch address, set for Thumb code
inline constexpr uint32_t THUMB_BIT = 1;
inline constexpr uint32_t ARM_INSTRUCTION_SIZE = 4;
inline constexpr uint32_t CP15 = 15;

// Instruction encodings with every operand field 0, except ARM_MOV_PC_LR. ARM encodings use the AL
// condition. A 32-bit Thumb encoding has its first halfword in the high bits.
inline constexpr uint16_t THUMB_NOP = 0xBF00;
inline constexpr uint16_t THUMB_WFI = 0xBF30;
inline constexpr uint16_t THUMB_BKPT = 0xBE00;
inline constexpr uint16_t THUMB_BX = 0x4700;
inline constexpr uint16_t THUMB_MOV_IMM = 0x2000;
inline constexpr uint32_t THUMB_MOVW = 0xF2400000U;
inline constexpr uint32_t THUMB_MOVT = 0xF2C00000U;
inline constexpr uint32_t THUMB_BLX_IMM = 0xF000C000U;
inline constexpr uint32_t ARM_BKPT = 0xE1200070U;
inline constexpr uint32_t ARM_SVC = 0xEF000000U;
inline constexpr uint32_t ARM_MOV_IMM = 0xE3A00000U;
inline constexpr uint32_t ARM_MOVW = 0xE3000000U;
inline constexpr uint32_t ARM_MOVT = 0xE3400000U;
inline constexpr uint32_t ARM_BX = 0xE12FFF10U;
inline constexpr uint32_t ARM_BLX_IMM = 0xFA000000U;
// MOV pc, lr
inline constexpr uint32_t ARM_MOV_PC_LR = 0xE1A0F00EU;

// AAPCS: arguments in r0 to r3, SP 8 byte aligned at a call, and d8 to d15 (s16 to s31) kept by the callee
inline constexpr uint32_t AAPCS_ARG_REG_COUNT = 4;
inline constexpr uint32_t AAPCS_STACK_ALIGN = 8;
inline constexpr uint32_t AAPCS_VFP_CALLEE_SAVED_FIRST_D = 8;
inline constexpr uint32_t AAPCS_VFP_CALLEE_SAVED_LAST_D = 15;
inline constexpr uint32_t AAPCS_VFP_CALLEE_SAVED_FIRST_S = AAPCS_VFP_CALLEE_SAVED_FIRST_D * WORDS_PER_D_REG;
inline constexpr uint32_t AAPCS_VFP_CALLEE_SAVED_S_COUNT = (AAPCS_VFP_CALLEE_SAVED_LAST_D + 1 - AAPCS_VFP_CALLEE_SAVED_FIRST_D) * WORDS_PER_D_REG;

// Instructions the encoders below can build
enum class Instruction : uint8_t {
    MOVW, // MOVW Rd, #imm
    MOVT, // MOVT Rd, #imm
    SYSCALL, // SVC #0
    BRANCH, // BX Rn
    BLX, // BLX imm
};

uint32_t encode_arm_inst(Instruction type, uint32_t immed, uint16_t reg);
// Returns 0 for SYSCALL
uint32_t encode_thumb_inst(Instruction type, uint32_t immed, uint16_t reg);

} // namespace arm
