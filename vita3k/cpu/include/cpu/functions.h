// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

struct MemState;

CPUStatePtr init_cpu(bool cpu_opt, SceUID thread_id, std::size_t processor_id, MemState &mem, CPUProtocolBase *protocol);
int run(CPUState &state);
int step(CPUState &state);
void stop(CPUState &state);
void set_thread_id(CPUState &state, SceUID thread_id);
SceUID get_thread_id(CPUState &state);
uint32_t read_reg(CPUState &state, size_t index);
float read_float_reg(CPUState &state, size_t index);
void write_float_reg(CPUState &state, size_t index, float value);
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
void load_context(CPUState &state, const CPUContext &ctx);
std::size_t get_processor_id(CPUState &state);
void invalidate_jit_cache(CPUState &state, Address start, size_t length);

uint32_t read_fpscr(CPUState &state);
void write_fpscr(CPUState &state, uint32_t value);
uint32_t read_cpsr(CPUState &state);
void write_cpsr(CPUState &state, uint32_t value);

uint32_t stack_alloc(CPUState &state, size_t size);
uint32_t stack_free(CPUState &state, size_t size);

ExclusiveMonitorPtr new_exclusive_monitor(int max_num_cores);
void free_exclusive_monitor(ExclusiveMonitorPtr monitor);
void clear_exclusive(ExclusiveMonitorPtr monitor, std::size_t core_num);

// Debugging helpers
std::string disassemble(CPUState &state, uint64_t at, bool thumb, uint16_t *insn_size = nullptr);
std::string disassemble(CPUState &state, uint64_t at, uint16_t *insn_size = nullptr);
bool hit_breakpoint(CPUState &state);
void trigger_breakpoint(CPUState &state);
void set_log_code(CPUState &state, bool log);
void set_log_mem(CPUState &state, bool log);
bool get_log_code(CPUState &state);
bool get_log_mem(CPUState &state);
