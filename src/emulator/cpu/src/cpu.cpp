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

#include <disasm/functions.h>
#include <disasm/state.h>
#include <mem/ptr.h>
#include <util/log.h>

#include <unicorn/unicorn.h>

#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>

typedef std::unique_ptr<uc_struct, std::function<void(uc_struct *)>> UnicornPtr;

struct CPUState {
    MemState *mem = nullptr;
    CallSVC call_svc;
    DisasmState disasm;
    UnicornPtr uc;
    Address entry_point;
};

static const bool LOG_CODE = true;
static const bool LOG_MEM_ACCESS = false;

static void delete_cpu_state(CPUState *state) {
    delete state;
}

static bool is_thumb_mode(uc_engine *uc) {
    size_t mode = 0;
    const uc_err err = uc_query(uc, UC_QUERY_MODE, &mode);
    assert(err == UC_ERR_OK);

    return mode & UC_MODE_THUMB;
}

static void code_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    CPUState &state = *static_cast<CPUState *>(user_data);
    const MemState &mem = *state.mem;
    const uint8_t *const code = Ptr<const uint8_t>(static_cast<Address>(address)).get(mem);
    const size_t buffer_size = GB(4) - address;
    const bool thumb = is_thumb_mode(uc);
    const std::string disassembly = disassemble(state.disasm, code, buffer_size, address, thumb);
    LOG_TRACE("{:#08x} {}", address, disassembly);
}

static void log_memory_access(const char *type, Address address, int size, int64_t value, const MemState &mem) {
    const char *const name = mem_name(address, mem);
    LOG_TRACE("{} {} bytes, address {:#08x} ( {} ), value {:#x}", type, size, address, name, value);
}

static void read_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    assert(value == 0);

    const CPUState &state = *static_cast<const CPUState *>(user_data);
    const MemState &mem = *state.mem;
    memcpy(&value, Ptr<const void>(static_cast<Address>(address)).get(mem), size);
    log_memory_access("Read", static_cast<Address>(address), size, value, mem);
}

static void write_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    const CPUState &state = *static_cast<const CPUState *>(user_data);
    const MemState &mem = *state.mem;
    log_memory_access("Write", static_cast<Address>(address), size, value, mem);
}

static void intr_hook(uc_engine *uc, uint32_t intno, void *user_data) {
    assert(intno == 2);

    CPUState &state = *static_cast<CPUState *>(user_data);

    uint32_t pc = 0;
    uc_err err = uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    assert(err == UC_ERR_OK);

    if (is_thumb_mode(uc)) {
        const Address svc_address = pc - 2;
        uint16_t svc_instruction = 0;
        err = uc_mem_read(uc, svc_address, &svc_instruction, sizeof(svc_instruction));
        assert(err == UC_ERR_OK);
        const uint8_t imm = svc_instruction & 0xff;
        state.call_svc(imm, pc);
    } else {
        const Address svc_address = pc - 4;
        uint32_t svc_instruction = 0;
        err = uc_mem_read(uc, svc_address, &svc_instruction, sizeof(svc_instruction));
        assert(err == UC_ERR_OK);
        const uint32_t imm = svc_instruction & 0xffffff;
        state.call_svc(imm, pc);
    }
}

static void enable_vfp_fpu(uc_engine *uc) {
    uint64_t c1_c0_2 = 0;
    uc_err err = uc_reg_read(uc, UC_ARM_REG_C1_C0_2, &c1_c0_2);
    assert(err == UC_ERR_OK);

    c1_c0_2 |= (0xf << 20);

    err = uc_reg_write(uc, UC_ARM_REG_C1_C0_2, &c1_c0_2);
    assert(err == UC_ERR_OK);

    const uint64_t fpexc = 0xf0000000;

    err = uc_reg_write(uc, UC_ARM_REG_FPEXC, &fpexc);
    assert(err == UC_ERR_OK);
}

CPUStatePtr init_cpu(Address pc, Address sp, bool log_code, CallSVC call_svc, MemState &mem) {
    CPUStatePtr state(new CPUState(), delete_cpu_state);
    state->mem = &mem;
    state->call_svc = call_svc;
    state->entry_point = pc;

    if (!init(state->disasm)) {
        return CPUStatePtr();
    }

    uc_engine *temp_uc = nullptr;
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &temp_uc);
    assert(err == UC_ERR_OK);

    state->uc = UnicornPtr(temp_uc, uc_close);
    temp_uc = nullptr;

    uc_hook hh = 0;
    if (log_code && LOG_CODE) {
        const uc_err err = uc_hook_add(state->uc.get(), &hh, UC_HOOK_CODE, reinterpret_cast<void *>(&code_hook), state.get(), 1, 0);
        assert(err == UC_ERR_OK);
    }

    if (LOG_MEM_ACCESS) {
        uc_err err = uc_hook_add(state->uc.get(), &hh, UC_HOOK_MEM_READ, reinterpret_cast<void *>(&read_hook), state.get(), 1, 0);
        assert(err == UC_ERR_OK);

        err = uc_hook_add(state->uc.get(), &hh, UC_HOOK_MEM_WRITE, reinterpret_cast<void *>(&write_hook), state.get(), 1, 0);
        assert(err == UC_ERR_OK);
    }

    err = uc_hook_add(state->uc.get(), &hh, UC_HOOK_INTR, reinterpret_cast<void *>(&intr_hook), state.get(), 1, 0);
    assert(err == UC_ERR_OK);

    err = uc_reg_write(state->uc.get(), UC_ARM_REG_SP, &sp);
    assert(err == UC_ERR_OK);

    err = uc_mem_map_ptr(state->uc.get(), 0, GB(4), UC_PROT_ALL, &mem.memory[0]);
    assert(err == UC_ERR_OK);

    err = uc_reg_write(state->uc.get(), UC_ARM_REG_PC, &pc);

    assert(err == UC_ERR_OK);

    err = uc_reg_write(state->uc.get(), UC_ARM_REG_LR, &pc);

    assert(err == UC_ERR_OK);

    enable_vfp_fpu(state->uc.get());

    return state;
}

int run(CPUState &state, bool callback) {
    std::uint32_t pc = read_pc(state);
    bool thumb_mode = is_thumb_mode(state.uc.get());
    if (thumb_mode) {
        pc |= 1;
    }
    if (callback) {
        uc_reg_write(state.uc.get(), UC_ARM_REG_LR, &state.entry_point);
    }
    uc_err err = uc_emu_start(state.uc.get(), pc, 0, 0, 1);
    pc = read_pc(state);
    thumb_mode = is_thumb_mode(state.uc.get());
    if (thumb_mode) {
        pc |= 1;
    }
    err = uc_emu_start(state.uc.get(), pc, state.entry_point & 0xfffffffe, 0, 0);

    if (err != UC_ERR_OK) {
        std::uint32_t error_pc = read_pc(state);
        uint32_t lr = read_lr(state);
        LOG_CRITICAL("Unicorn error {:#02x} at: start PC: {:#08x} error PC {:#08x} LR: {:#08x}", err, pc, error_pc, lr);
        return -1;
    }
    pc = read_pc(state);
    thumb_mode = is_thumb_mode(state.uc.get());
    if (thumb_mode) {
        pc |= 1;
    }
    if (pc == state.entry_point)
        return 1;
    return 0;
}

void stop(CPUState &state) {
    const uc_err err = uc_emu_stop(state.uc.get());
    assert(err == UC_ERR_OK);
}

uint32_t read_reg(CPUState &state, size_t index) {
    assert(index >= 0);
    assert(index <= 3);

    uint32_t value = 0;
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_R0 + index, &value);
    assert(err == UC_ERR_OK);

    return value;
}

uint32_t read_sp(CPUState &state) {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_SP, &value);
    assert(err == UC_ERR_OK);

    return value;
}

uint32_t read_pc(CPUState &state) {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_PC, &value);
    assert(err == UC_ERR_OK);

    return value;
}

uint32_t read_lr(CPUState &state) {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_LR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void write_reg(CPUState &state, size_t index, uint32_t value) {
    assert(index >= 0);
    assert(index <= 1);

    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_R0 + index, &value);
    assert(err == UC_ERR_OK);
}

void write_sp(CPUState &state, uint32_t value) {
    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_SP, &value);
    assert(err == UC_ERR_OK);
}

void write_pc(CPUState &state, uint32_t value) {
    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_PC, &value);
    assert(err == UC_ERR_OK);
}

void write_lr(CPUState &state, uint32_t value) {
    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_LR, &value);
    assert(err == UC_ERR_OK);
}
