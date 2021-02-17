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

#include <cstdint>
#include <functional>
#include <stack>

struct CPUState;
struct MemState;

enum ImportCallLogLevel {
    None,
    LogCall,
    LogCallAndReturn
};

constexpr ImportCallLogLevel IMPORT_CALL_LOG_LEVEL = None;

struct CPUContext {
    uint32_t cpu_registers[16];
    uint32_t sp;
    uint32_t pc;
    uint32_t lr;
    uint32_t cpsr;
};

struct StackFrame {
    uint32_t addr;
    uint32_t sp;
    std::string name;
};

struct ModuleRegion {
    uint32_t nid;
    std::string name;
    Address start;
    uint32_t size;
    Address vaddr;
};

typedef std::function<void(CPUState &cpu, uint32_t, Address)> CallSVC;
typedef std::function<void(CPUState &cpu, uint32_t, SceUID)> CallImport;
typedef std::function<std::string(Address)> ResolveNIDName;
typedef std::function<Address(Address)> GetWatchMemoryAddr;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::unique_ptr<CPUContext, std::function<void(CPUContext *)>> CPUContextPtr;

struct CPUDepInject {
    CallImport call_import;
    CallSVC call_svc;
    ResolveNIDName resolve_nid_name;
    GetWatchMemoryAddr get_watch_memory_addr;
    std::vector<ModuleRegion> module_regions;
    bool trace_stack;
};

CPUStatePtr init_cpu(SceUID thread_id, Address pc, Address sp, MemState &mem, CPUDepInject &inject);
int run(CPUState &state, bool callback, Address entry_point);
int step(CPUState &state, bool callback, Address entry_point);
void stop(CPUState &state);
uint32_t read_reg(CPUState &state, size_t index);
float read_float_reg(CPUState &state, size_t index);
uint32_t read_sp(CPUState &state);
uint32_t read_pc(CPUState &state);
uint32_t read_lr(CPUState &state);
uint32_t read_fpscr(CPUState &state);
uint32_t read_cpsr(CPUState &state);
uint32_t read_tpidruro(CPUState &state);
void write_reg(CPUState &state, size_t index, uint32_t value);
void write_float_reg(CPUState &state, size_t index, float value);
void write_sp(CPUState &state, uint32_t value);
void write_pc(CPUState &state, uint32_t value);
void write_lr(CPUState &state, uint32_t value);
void write_fpscr(CPUState &state, uint32_t value);
void write_cpsr(CPUState &state, uint32_t value);
void write_tpidruro(CPUState &state, uint32_t value);

// Debugging helpers
std::string disassemble(CPUState &state, uint64_t at, bool thumb, uint16_t *insn_size = nullptr);
std::string disassemble(CPUState &state, uint64_t at, uint16_t *insn_size = nullptr);
bool hit_breakpoint(CPUState &state);
void trigger_breakpoint(CPUState &state);
void log_code_add(CPUState &state);
void log_mem_add(CPUState &state);
void log_code_remove(CPUState &state);
void log_mem_remove(CPUState &state);
bool log_code_exists(CPUState &state);
bool log_mem_exists(CPUState &state);
void save_context(CPUState &state, CPUContext &ctx);
void load_context(CPUState &state, CPUContext &ctx);
std::stack<StackFrame> get_stack_frames(CPUState &state);
void push_stack_frame(CPUState &state, StackFrame sf);
void log_stack_frames(CPUState &cpu);
bool is_returning(CPUState &cpu);
void set_thread_id(CPUState &state, SceUID thread_id);
SceUID get_thread_id(CPUState &state);

void push_lr(CPUState &cpu, Address lr);
Address pop_lr(CPUState &cpu);
