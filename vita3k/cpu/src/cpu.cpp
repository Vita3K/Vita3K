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

#include <cpu/disasm/functions.h>
#include <cpu/functions.h>
#include <cpu/impl/dynarmic_cpu.h>
#include <cpu/impl/interface.h>
#include <cpu/state.h>
#include <mem/ptr.h>
#include <util/types.h>

#include <memory>
#include <string>

static void delete_cpu_state(CPUState *state) {
    delete state;
}

void set_thread_id(CPUState &state, SceUID thread_id) {
    state.thread_id = thread_id;
}

SceUID get_thread_id(CPUState &state) {
    return state.thread_id;
}

CPUStatePtr init_cpu(bool cpu_opt, SceUID thread_id, std::size_t processor_id, MemState &mem, CPUProtocolBase *protocol) {
    CPUStatePtr state(new CPUState(), delete_cpu_state);
    state->mem = &mem;
    state->protocol = protocol;
    state->thread_id = thread_id;

    // TODO: we can move this to kernel after we drop unicorn
    // unicorn is unable to detect whether the exit was because of halt or not
    state->halt_instruction = alloc_block(mem, 4, "halt_instruction");
    const auto halt_ptr = state->halt_instruction.get_ptr<uint16_t>();
    *halt_ptr.get(mem) = 0xBF00; // NOP
    *(halt_ptr.get(mem) + 1) = 0xBF30; // WFI
    state->halt_instruction_pc = state->halt_instruction.get() | 1;

    if (!init(state->disasm)) {
        return CPUStatePtr();
    }

    Dynarmic::ExclusiveMonitor *monitor = static_cast<Dynarmic::ExclusiveMonitor *>(protocol->get_exclusive_monitor());
    state->cpu = std::make_unique<DynarmicCPU>(state.get(), processor_id, monitor, cpu_opt);

    return state;
}

int run(CPUState &state) {
    return state.cpu->run();
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

std::size_t get_processor_id(CPUState &state) {
    return state.cpu->processor_id();
}

void invalidate_jit_cache(CPUState &state, Address start, size_t length) {
    state.cpu->invalidate_jit_cache(start, length);
}

std::string disassemble(CPUState &state, uint64_t at, bool thumb, uint16_t *insn_size) {
    MemState &mem = *state.mem;
    const uint8_t *const code = Ptr<const uint8_t>(static_cast<Address>(at)).get(mem);
    const size_t buffer_size = GiB(4) - at;
    return disassemble(state.disasm, code, buffer_size, at, thumb, insn_size);
}

std::string disassemble(CPUState &state, uint64_t at, uint16_t *insn_size) {
    const bool thumb = state.cpu->is_thumb_mode();
    return disassemble(state, at, thumb, insn_size);
}

CPUContext save_context(CPUState &state) {
    return state.cpu->save_context();
}

void load_context(CPUState &state, const CPUContext &ctx) {
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
