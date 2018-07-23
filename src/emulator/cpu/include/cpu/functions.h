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

#include <cpu/factory.h>
#include <disasm/functions.h>
#include <disasm/state.h>
#include <mem/mem.h> // Address.

#include <cstdint>
#include <functional>

struct MemState;
struct CPUState;

typedef std::function<void(CPUState &cpu, uint32_t, Address)> CallSVC;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;

union DoubleReg {
    double d;
    float f[2];
};

struct CPUState {
    MemState *mem = nullptr;
    CallSVC call_svc;
    DisasmState disasm;
    CPUInterfacePtr cpu;
    Address entry_point;
};

CPUStatePtr init_cpu(CPUBackend backend, Address pc, Address sp, bool log_code, CallSVC call_svc, MemState &mem);
int run(CPUState &state, bool callback);
void stop(CPUState &state);
uint32_t read_reg(CPUState &state, size_t index);
float read_float_reg(CPUState &state, size_t index);
uint32_t read_sp(CPUState &state);
uint32_t read_pc(CPUState &state);
uint32_t read_lr(CPUState &state);
void write_reg(CPUState &state, size_t index, uint32_t value);
void write_sp(CPUState &state, uint32_t value);
void write_pc(CPUState &state, uint32_t value);
void write_lr(CPUState &state, uint32_t value);
