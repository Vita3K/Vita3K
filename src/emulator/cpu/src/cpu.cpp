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

#include <unicorn/unicorn.h>

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

    state->cpu = new_cpu(backend);
}

int run(CPUState &state, bool callback) {
    state.cpu->run();
    return 0;
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
