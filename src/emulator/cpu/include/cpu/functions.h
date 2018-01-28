// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <mem/mem.h> // Address.

#include <cstdint>
#include <functional>

struct CPUState;
struct MemState;

typedef std::function<void(uint32_t, Address)> CallSVC;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;

CPUStatePtr init_cpu(Address pc, Address sp, bool log_code, CallSVC call_svc, MemState &mem);
bool run(CPUState &state);
void stop(CPUState &state);
uint32_t read_reg(CPUState &state, size_t index);
uint32_t read_sp(CPUState &state);
void write_reg(CPUState &state, size_t index, uint32_t value);
void write_pc(CPUState &state, uint32_t value);
