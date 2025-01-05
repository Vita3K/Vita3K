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
#include <string>
#include <util/types.h>
#include <vector>

#include "patch/instructions.h"
#include "patch/patch.h"

PatchHeader read_header(std::string &header, bool isPatchlist);
std::vector<uint8_t> to_bytes(unsigned long long value, uint8_t count);

void strip_arg_spaces(std::string &line);
void strip_arg_spaces(std::string &line, char open, char close);

Instruction to_instruction(const std::string &inst);
bool is_valid_instruction(std::string &inst);
std::string strip_args(std::string inst);

std::vector<std::string> get_args(std::string inst, char open, char close);
std::vector<std::string> get_args(std::string inst);

uint32_t translate(std::string &inst, std::vector<uint32_t> &args);
