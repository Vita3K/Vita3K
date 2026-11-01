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

#include <mem/util.h> // Address.
#include <util/types.h>

#include <cpu/common.h>

#include <cstdint>
#include <initializer_list>

struct MemState;

CPUStatePtr init_cpu(bool cpu_opt, SceUID thread_id, std::size_t processor_id, MemState &mem);
CPUStop run(CPUState &state);
CPUStop step(CPUState &state);
// Sets up a call of fn with args: the first four in r0 to r3, the rest on the stack, and the
// return address in LR
void place_call(CPUState &state, Address fn, std::initializer_list<uint32_t> args);
void stop(CPUState &state);
uint32_t read_reg(CPUState &state, size_t index);
float read_float_reg(CPUState &state, size_t index);
void write_float_reg(CPUState &state, size_t index, float value);
uint32_t read_sp(CPUState &state);
uint32_t read_pc(CPUState &state);
uint32_t read_lr(CPUState &state);
uint32_t read_tpidruro(CPUState &state);
uint32_t read_tpidrurw(CPUState &state);
void write_reg(CPUState &state, size_t index, uint32_t value);
void write_sp(CPUState &state, uint32_t value);
void write_pc(CPUState &state, uint32_t value);
void write_lr(CPUState &state, uint32_t value);
void write_tpidruro(CPUState &state, uint32_t value);
void write_tpidrurw(CPUState &state, uint32_t value);
CPUContext save_context(CPUState &state);
void load_context(CPUState &state, const CPUContext &ctx);
std::size_t get_processor_id(CPUState &state);
void invalidate_jit_cache(CPUState &state, Address start, size_t length);

uint32_t read_fpscr(CPUState &state);
void write_fpscr(CPUState &state, uint32_t value);
uint32_t read_cpsr(CPUState &state);
void write_cpsr(CPUState &state, uint32_t value);

uint32_t stack_alloc(CPUState &state, size_t size);
void stack_free(CPUState &state, size_t size);

void clear_exclusive(CPUState &state);

// Debugging helpers
std::string disassemble(CPUState &state, uint64_t at, bool thumb, uint16_t *insn_size = nullptr);
std::string disassemble(CPUState &state, uint64_t at, uint16_t *insn_size = nullptr);
bool hit_breakpoint(CPUState &state);
void trigger_breakpoint(CPUState &state);
void set_log_code(CPUState &state, bool log);

void set_log_mem(CPUState &state, bool log);
bool get_log_mem(CPUState &state);

// The CPU state running on this host thread, for signal handlers
void set_current_cpu_state(CPUState *state);
CPUState *get_current_cpu_state();
