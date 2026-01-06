// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <array>
#include <map>
#include <string>
#include <util/types.h>
#include <vector>

using TranslateFn = uint32_t (*)(std::vector<uint32_t> &args);

enum class Instruction {
    NOP,
    T1_MOV,

    // All-encompassing "this is not an instruction" value
    INVALID,
};

struct Op {
    Instruction instruction;
    TranslateFn translate;
};

/**
 * Special instructions
 */
uint32_t nop(std::vector<uint32_t> &args);

/**
 * MOV instructions
 *
 * https://developer.arm.com/documentation/ddi0406/c/Application-Level-Architecture/Instruction-Details/Alphabetical-list-of-instructions/MOV--immediate-?lang=en
 */
uint32_t t1_mov(std::vector<uint32_t> &args);

static const std::map<std::string, Op> instruction_funcs = {
    { "nop", { Instruction::NOP, nop } },
    { "t1_mov", { Instruction::T1_MOV, t1_mov } },
};
