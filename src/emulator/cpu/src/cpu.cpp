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

#include <cpu/factory.h>
#include <cpu/functions.h>
#include <cpu/interface.h>

#include <disasm/functions.h>
#include <disasm/state.h>
#include <mem/ptr.h>
#include <util/log.h>

#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>

static void delete_cpu_state(CPUState *state) {
    delete state;
}

CPUStatePtr init_cpu(CPUBackend backend, Address pc, Address sp, bool log_code, CallSVC call_svc, MemState &mem) {
    CPUStatePtr state(new CPUState(), delete_cpu_state);
    state->mem = &mem;
    state->call_svc = call_svc;
    state->entry_point = pc;

    if (!init(state->disasm)) {
        return CPUStatePtr();
    }

    state->cpu = new_cpu(backend, pc, sp, log_code, state.get());
    return state;
}

static int do_run(CPUState &state, bool callback, bool is_step) {
    if (callback) {
        state.cpu->set_lr(state.entry_point);
    }

    const int err = is_step ? state.cpu->step() : state.cpu->run();

    if (err < 0) {
        return err;
    }
    
    uint32_t pc = state.cpu->get_pc();
    bool thumb_mode = state.cpu->is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    return pc == state.entry_point;
}

int run(CPUState &state, bool callback) {
    return do_run(state, callback, false);
}

int step(CPUState &state, bool callback) {
    return do_run(state, callback, true);
}

void stop(CPUState &state) {
    state.cpu->stop();
}

uint32_t read_reg(CPUState &state, size_t index) {
    return state.cpu->get_reg(static_cast<uint8_t>(index));
}

float read_float_reg(CPUState &state, size_t index) {
    return state.cpu->get_float_reg(static_cast<uint8_t>(index));
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

void write_reg(CPUState &state, size_t index, uint32_t value) {
    state.cpu->set_reg(static_cast<uint8_t>(index), value);
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

bool hit_breakpoint(CPUState &state) {
    return state.did_break;
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

