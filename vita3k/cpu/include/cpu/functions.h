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
#include <util/types.h>

#include <cpu/common.h>

#include <cstdint>
#include <functional>
#include <stack>

enum ImportCallLogLevel {
    None,
    LogCall,
    LogCallAndReturn
};

constexpr ImportCallLogLevel IMPORT_CALL_LOG_LEVEL = None;

struct StackFrame {
    uint32_t addr;
    uint32_t sp;
    std::string name;
};

CPUStatePtr init_cpu(CPUBackend backend, SceUID thread_id, Address pc, Address sp, MemState &mem, CPUDepInject &inject);
// run a guest function
// if this is called inside interrupt handler
// it will not modify current cpu state
int run(CPUState &state, Address entry_point);
// run a guest function inside another cpu instance
// this does not modify the current cpu state
CPUContext run_worker(CPUState &state, CPUBackend backend, Address pc);
int step(CPUState &state, Address entry_point);
void stop(CPUState &state);
uint32_t read_reg(CPUState &state, size_t index);
float read_float_reg(CPUState &state, size_t index);
uint32_t read_sp(CPUState &state);
uint32_t read_pc(CPUState &state);
uint32_t read_lr(CPUState &state);
uint32_t read_tpidruro(CPUState &state);
void write_reg(CPUState &state, size_t index, uint32_t value);
void write_sp(CPUState &state, uint32_t value);
void write_pc(CPUState &state, uint32_t value);
void write_lr(CPUState &state, uint32_t value);
void write_tpidruro(CPUState &state, uint32_t value);
bool is_thumb_mode(CPUState &state);
CPUContext save_context(CPUState &state);
void load_context(CPUState &state, CPUContext ctx);

// Debugging helpers
std::string disassemble(CPUState &state, uint64_t at, bool thumb, uint16_t *insn_size = nullptr);
std::string disassemble(CPUState &state, uint64_t at, uint16_t *insn_size = nullptr);
bool hit_breakpoint(CPUState &state);
void trigger_breakpoint(CPUState &state);
void set_log_code(CPUState &state, bool log);
void set_log_mem(CPUState &state, bool log);
bool get_log_code(CPUState &state);
bool get_log_mem(CPUState &state);
bool is_returning(CPUState &cpu);

std::unique_ptr<ModuleRegion> get_region(CPUState &state, Address addr);