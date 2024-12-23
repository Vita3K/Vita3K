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

#include "patch/instructions.h"

#include <util/log.h>

bool isValidInstruction(std::string &inst) {
  return toInstruction(stripArgs(inst)) != Instruction::INVALID;
}

std::string stripArgs(std::string inst) {
  auto open = inst.find('(');
  auto close = inst.find(')');

  if (open == std::string::npos || close == std::string::npos)
    return inst;

  inst.erase(open, close - open + 1);

  return inst;
}

std::vector<uint32_t> getArgs(std::string inst) {
  auto open = inst.find('(');
  auto close = inst.find(')');
  std::vector<uint32_t> args;
  size_t pos = 0;

  if (open == std::string::npos || close == std::string::npos)
    return args;

  inst = inst.substr(open + 1, close - open - 1);

  // If there is only one value, set pos to the end of the string
  if ((pos = inst.find(',')) == std::string::npos)
    pos = inst.length() - 1;

  do {
    pos = inst.find(',');
    std::string val = inst.substr(0, pos);

    args.push_back(std::stoull(val, nullptr, 16));
    inst.erase(0, pos + 1);
  } while (pos != std::string::npos);

  return args;
}

uint32_t translate(Instruction &inst, std::vector<uint32_t> &args) {
  TranslateFn f = instruction_funcs[static_cast<int>(inst)];
  return f(args);
}

uint32_t nop(std::vector<uint32_t> &args) {
  return 0;
}

uint32_t t1_mov(std::vector<uint32_t> &args) {
  uint8_t b0 = args[1];
  uint8_t b1 = 0b00100000 | (args[0] << 8);

  return (b0 << 8) | b1;
}