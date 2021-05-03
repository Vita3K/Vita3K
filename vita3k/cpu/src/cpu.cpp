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

#include <cpu/functions.h>
#include <cpu/interface.h>

#include <disasm/functions.h>
#include <disasm/state.h>
#include <mem/ptr.h>
#include <util/log.h>
#include <util/types.h>

#include <cpu/dynarmic_cpu.h>
#include <cpu/unicorn_cpu.h>

#include <cassert>
#include <cpu/state.h>
#include <cstring>

#include <spdlog/fmt/fmt.h>
#include <util/string_utils.h>

static void delete_cpu_state(CPUState *state) {
    delete state;
}

void set_thread_id(CPUState &state, SceUID thread_id) {
    state.thread_id = thread_id;
}

SceUID get_thread_id(CPUState &state) {
    return state.thread_id;
}

CPUStatePtr init_cpu(CPUBackend backend, SceUID thread_id, Address pc, Address sp, MemState &mem, CPUDepInject &inject) {
    CPUStatePtr state(new CPUState(), delete_cpu_state);
    state->mem = &mem;
    state->call_svc = inject.call_svc;
    state->resolve_nid_name = inject.resolve_nid_name;
    state->get_watch_memory_addr = inject.get_watch_memory_addr;
    state->thread_id = thread_id;
    state->module_regions = inject.module_regions;
    Ptr<uint16_t> halt_inst = Ptr<uint16_t>(alloc(mem, 4, "halt_instruction"));
    *halt_inst.get(mem) = 0xBF00; // NOP
    *(halt_inst.get(mem) + 1) = 0xBF30; // WFI
    state->halt_instruction_pc = halt_inst.address() | 1;

    if (!init(state->disasm)) {
        return CPUStatePtr();
    }

    switch (backend) {
    case CPUBackend::Dynarmic: {
        state->cpu = std::make_unique<DynarmicCPU>(state.get(), pc, sp, false);
        break;
    }
    case CPUBackend::Unicorn: {
        state->cpu = std::make_unique<UnicornCPU>(state.get(), pc, sp, inject.trace_stack);
        break;
    }
    default:
        return nullptr;
    }

    return state;
}

int run(CPUState &state) {
    return state.cpu->run();
}

CPUContext run_worker(CPUState &state, CPUBackend backend) {
    CPUInterfacePtr newcpu;
    switch (backend) {
    case CPUBackend::Dynarmic: {
        newcpu = std::make_unique<DynarmicCPU>(&state, read_pc(state), read_sp(state), false);
        break;
    }
    case CPUBackend::Unicorn: {
        newcpu = std::make_unique<UnicornCPU>(&state, read_pc(state), read_sp(state), false);
        break;
    }
    }
    auto original_cpu = std::move(state.cpu);
    CPUContext context = original_cpu->save_context();
    newcpu->set_tpidruro(original_cpu->get_tpidruro());
    newcpu->load_context(context);
    state.cpu = std::move(newcpu);
    run(state);
    auto out = save_context(state);
    state.cpu = std::move(original_cpu);
    return out;
}

int step(CPUState &state) {
    return state.cpu->step();
}

void stop(CPUState &state) {
    state.cpu->stop();
}

uint32_t read_reg(CPUState &state, size_t index) {
    return state.cpu->get_reg(index);
}

float read_float_reg(CPUState &state, size_t index) {
    return state.cpu->get_float_reg(index);
}

void write_float_reg(CPUState &state, size_t index, float value) {
    state.cpu->set_float_reg(index, value);
}

void write_fpscr(CPUState &state, uint32_t value) {
    state.cpu->set_fpscr(value);
}

void write_cpsr(CPUState &state, uint32_t value) {
    state.cpu->set_cpsr(value);
}

uint32_t read_sp(CPUState &state) {
    return state.cpu->get_sp();
}

uint32_t read_pc(CPUState &state) {
    return state.cpu->get_pc();
}

uint32_t read_lr(CPUState &state) {
    return state.cpu->get_lr();
}

uint32_t read_fpscr(CPUState &state) {
    return state.cpu->get_fpscr();
}

uint32_t read_cpsr(CPUState &state) {
    return state.cpu->get_cpsr();
}

uint32_t read_tpidruro(CPUState &state) {
    return state.cpu->get_tpidruro();
}

void write_reg(CPUState &state, size_t index, uint32_t value) {
    state.cpu->set_reg(index, value);
}

void write_sp(CPUState &state, uint32_t value) {
    state.cpu->set_sp(value);
}

void write_pc(CPUState &state, uint32_t value) {
    state.cpu->set_pc(value);
}

void write_lr(CPUState &state, uint32_t value) {
    state.cpu->set_lr(value);
}

void write_tpidruro(CPUState &state, uint32_t value) {
    state.cpu->set_tpidruro(value);
}

bool is_thumb_mode(CPUState &state) {
    return state.cpu->is_thumb_mode();
}

bool hit_breakpoint(CPUState &state) {
    return state.cpu->hit_breakpoint();
}

void trigger_breakpoint(CPUState &state) {
    state.cpu->trigger_breakpoint();
}

void set_log_code(CPUState &state, bool log) {
    state.cpu->set_log_code(log);
}

void set_log_mem(CPUState &state, bool log) {
    state.cpu->set_log_mem(log);
}

bool get_log_code(CPUState &state) {
    return state.cpu->get_log_code();
}

bool get_log_mem(CPUState &state) {
    return state.cpu->get_log_mem();
}

bool is_returning(CPUState &state) {
    return state.cpu->is_returning();
}

std::unique_ptr<ModuleRegion> get_region(CPUState &state, Address addr) {
    for (const auto &region : state.module_regions) {
        if (region.start <= addr && addr < region.start + region.size) {
            return std::make_unique<ModuleRegion>(region);
        }
    }
    return nullptr;
}

std::string disassemble(CPUState &state, uint64_t at, bool thumb, uint16_t *insn_size) {
    MemState &mem = *state.mem;
    const uint8_t *const code = Ptr<const uint8_t>(static_cast<Address>(at)).get(mem);
    const size_t buffer_size = GB(4) - at;
    return disassemble(state.disasm, code, buffer_size, at, thumb, insn_size);
}

std::string disassemble(CPUState &state, uint64_t at, uint16_t *insn_size) {
    const bool thumb = state.cpu->is_thumb_mode();
    return disassemble(state, at, thumb, insn_size);
}

CPUContext save_context(CPUState &state) {
    return state.cpu->save_context();
}

void load_context(CPUState &state, CPUContext ctx) {
    state.cpu->load_context(ctx);
}

uint32_t stack_alloc(CPUState &state, size_t size) {
    const uint32_t new_sp = read_sp(state) - size;
    write_sp(state, new_sp);
    return new_sp;
}

uint32_t stack_free(CPUState &state, size_t size) {
    const uint32_t new_sp = read_sp(state) + size;
    write_sp(state, new_sp);
    return new_sp;
}
