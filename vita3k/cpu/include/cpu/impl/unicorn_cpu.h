// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <cpu/impl/interface.h>

#include <cpu/disasm/state.h>
#include <cpu/state.h>
#include <unicorn/unicorn.h>

#include <array>
#include <functional>
#include <memory>

typedef std::unique_ptr<uc_struct, std::function<void(uc_struct *)>> UnicornPtr;

struct CPUState;

/*! \brief Base class for all CPU backend implementation */
class UnicornCPU : public CPUInterface {
    UnicornPtr uc;

    CPUState *parent;

    Address ep;
    uc_hook memory_read_hook_handle = 0;
    uc_hook memory_write_hook_handle = 0;
    uc_hook code_hook_handle = 0;

    bool is_inside_intr_hook = false;

    std::stack<Address> lr_stack;

    bool returning = false;
    bool did_break = false;
    bool did_inject = false;

    void log_error_details(uc_err code);

    static void intr_hook(uc_engine *uc, uint32_t intno, void *user_data);
    static void code_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data);
    static void read_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data);
    static void write_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data);
    void log_memory_access(uc_engine *uc, const char *type, Address address, int size, int64_t value, MemState &mem, CPUState &cpu, Address offset);
    int run_after_injected(uint32_t pc, bool thumb_mode);

public:
    UnicornCPU(CPUState *cpu);

    int execute_instructions_no_check(int num);

    int run() override;
    int step() override;

    void stop() override;

    uint32_t get_reg(uint8_t idx) override;
    void set_reg(uint8_t idx, uint32_t val) override;

    uint32_t get_sp() override;
    void set_sp(uint32_t val) override;

    uint32_t get_pc() override;
    void set_pc(uint32_t val) override;

    uint32_t get_lr() override;
    void set_lr(uint32_t val) override;

    uint32_t get_cpsr() override;
    void set_cpsr(uint32_t val) override;

    uint32_t get_tpidruro() override;
    void set_tpidruro(uint32_t val) override;

    uint32_t get_fpscr() override;
    void set_fpscr(uint32_t val) override;

    float get_float_reg(uint8_t idx) override;
    void set_float_reg(uint8_t idx, float val) override;

    bool is_thumb_mode() override;

    CPUContext save_context() override;
    void load_context(CPUContext context) override;

    bool hit_breakpoint() override;
    void trigger_breakpoint() override;
    void set_log_code(bool log) override;
    void set_log_mem(bool log) override;
    bool get_log_code() override;
    bool get_log_mem() override;
};
