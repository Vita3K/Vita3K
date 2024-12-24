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

#include "instructions.h"

std::string readBinFromHeader(const std::string &header);
std::vector<uint8_t> toBytes(unsigned long long value, uint8_t count);
void removeInstructionSpaces(std::string &patchline);

Instruction toInstruction(const std::string &inst);
bool isValidInstruction(std::string &inst);
std::string stripArgs(std::string inst);
std::vector<std::string> getArgs(std::string inst, char open, char close);
std::vector<std::string> getArgs(std::string inst);

uint32_t translate(std::string &inst, std::vector<uint32_t> &args);
