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

union DoubleReg {
    double d;
    float f[2];
};

struct CPUState {
    MemState *mem = nullptr;
    CallSVC call_svc;
    DisasmState disasm;
    UnicornPtr uc;
    Address entry_point;
    bool did_break = false;
};

// Log code for specified threads (arg to create_thread)
static const bool LOG_CODE = true;
static const bool TRACE_RETURN_VALUES = true;

// Log code run on all threads
static bool LOG_CODE_ALL = false;
static bool LOG_MEM_ACCESS = false;

static void delete_cpu_state(CPUState *state) {
    delete state;
}

static bool is_thumb_mode(uc_engine *uc) {
    size_t mode = 0;
    const uc_err err = uc_query(uc, UC_QUERY_MODE, &mode);
    assert(err == UC_ERR_OK);

    return mode & UC_MODE_THUMB;
}

static inline void func_trace(CPUState &state) {
    if (TRACE_RETURN_VALUES)
        if (is_returning(state.disasm))
            LOG_TRACE("Returning, r0: {}", log_hex(read_reg(state, 0)));
}

static void code_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    CPUState &state = *static_cast<CPUState *>(user_data);
    std::string disassembly = disassemble(state, address);
    LOG_TRACE("{}: {} {}", log_hex((uint64_t)uc), log_hex(address), disassembly);

    func_trace(state);
}

static void log_memory_access(uc_engine *uc, const char *type, Address address, int size, int64_t value, MemState &mem) {
    const char *const name = mem_name(address, mem);
        LOG_TRACE("{}: {} {} bytes, address {} ( {} ), value {}", log_hex((uint64_t)uc), type, size, log_hex(address), name, log_hex(value));
}

static void read_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    assert(value == 0);

    CPUState &state = *static_cast<CPUState *>(user_data);
    MemState &mem = *state.mem;
    memcpy(&value, Ptr<const void>(static_cast<Address>(address)).get(mem), size);
    log_memory_access(uc, "Read", static_cast<Address>(address), size, value, mem);
}

static void write_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    CPUState &state = *static_cast<CPUState *>(user_data);
    MemState &mem = *state.mem;
    log_memory_access(uc, "Write", static_cast<Address>(address), size, value, mem);
}

constexpr uint32_t INT_SVC = 2;
constexpr uint32_t INT_BKPT = 7;

static void intr_hook(uc_engine *uc, uint32_t intno, void *user_data) {
    assert(intno == INT_SVC || intno == INT_BKPT);

    CPUState &state = *static_cast<CPUState *>(user_data);

    uint32_t pc = read_pc(state);

    switch (intno) {
    case INT_SVC:
        if (is_thumb_mode(uc)) {
            const Address svc_address = pc - 2;
            uint16_t svc_instruction = 0;
            uc_err err = uc_mem_read(uc, svc_address, &svc_instruction, sizeof(svc_instruction));
            assert(err == UC_ERR_OK);
            const uint8_t imm = svc_instruction & 0xff;
            state.call_svc(state, imm, pc);
        } else {
            const Address svc_address = pc - 4;
            uint32_t svc_instruction = 0;
            uc_err err = uc_mem_read(uc, svc_address, &svc_instruction, sizeof(svc_instruction));
            assert(err == UC_ERR_OK);
            const uint32_t imm = svc_instruction & 0xffffff;
            state.call_svc(state, imm, pc);
        }
        break;
    case INT_BKPT:
        stop(state);
        state.did_break = true;
        break;
    default: break;
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

static void log_error_details(CPUState &state, uc_err code) {
    // I don't especially want the time logged for every line, but I also want it to print to the log file...
    LOG_ERROR("Unicorn error {}. {}", log_hex(code), uc_strerror(code));

    uint32_t pc = read_pc(state);
    uint32_t sp = read_sp(state);
    uint32_t lr = read_lr(state);
    uint32_t registers[13];
    for (size_t a = 0; a < 13; a++)
        registers[a] = read_reg(state, a);

    LOG_ERROR("Executing: {}", disassemble(state, pc));
    LOG_ERROR("PC: 0x{:0>8x},   SP: 0x{:0>8x},   LR: 0x{:0>8x}", pc, sp, lr);
    for (int a = 0; a < 6; a++) {
        LOG_ERROR("r{: <2}: 0x{:0>8x}   r{: <2}: 0x{:0>8x}", a, registers[a], a + 6, registers[a + 6]);
    }
    LOG_ERROR("r12: 0x{:0>8x}", registers[12]);
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

    if ((log_code && LOG_CODE) || LOG_CODE_ALL) {
        log_code_add(*state);
    }

    if (LOG_MEM_ACCESS) {
        log_mem_add(*state);
    }

    err = uc_hook_add(state->uc.get(), &hh, UC_HOOK_INTR, reinterpret_cast<void *>(&intr_hook), state.get(), 1, 0);
    assert(err == UC_ERR_OK);

    err = uc_reg_write(state->uc.get(), UC_ARM_REG_SP, &sp);
    assert(err == UC_ERR_OK);

    // Don't map the null page into unicorn so that unicorn returns access error instead of
    // crashing the whole emulator on invalid access
    err = uc_mem_map_ptr(state->uc.get(), mem.page_size, GB(4) - mem.page_size, UC_PROT_ALL, &mem.memory[mem.page_size]);
    assert(err == UC_ERR_OK);

    err = uc_reg_write(state->uc.get(), UC_ARM_REG_PC, &pc);
    assert(err == UC_ERR_OK);

    err = uc_reg_write(state->uc.get(), UC_ARM_REG_LR, &pc);
    assert(err == UC_ERR_OK);

    enable_vfp_fpu(state->uc.get());

    return state;
}

int run(CPUState &state, bool callback) {
    state.did_break = false;
    uint32_t pc = read_pc(state);
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
        log_error_details(state, err);
#ifdef USE_GDBSTUB
        trigger_breakpoint(state);
        return 0;
#else
        return -1;
#endif
    }
    pc = read_pc(state);
    thumb_mode = is_thumb_mode(state.uc.get());
    if (thumb_mode) {
        pc |= 1;
    }

    return pc == state.entry_point;
}

int step(CPUState &state, bool callback) {
    uint32_t pc = read_pc(state);
    bool thumb_mode = is_thumb_mode(state.uc.get());
    if (thumb_mode) {
        pc |= 1;
    }
    if (callback) {
        uc_reg_write(state.uc.get(), UC_ARM_REG_LR, &state.entry_point);
    }

    uc_err err = uc_emu_start(state.uc.get(), pc, 0, 0, 1);

    if (err != UC_ERR_OK) {
        log_error_details(state, err);
        return -1;
    }
    pc = read_pc(state);
    thumb_mode = is_thumb_mode(state.uc.get());
    if (thumb_mode) {
        pc |= 1;
    }

    return pc == state.entry_point;
}

void stop(CPUState &state) {
    const uc_err err = uc_emu_stop(state.uc.get());
    assert(err == UC_ERR_OK);
}

uint32_t read_reg(CPUState &state, size_t index) {
    assert(index >= 0);

    uint32_t value = 0;
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_R0 + static_cast<int>(index), &value);
    assert(err == UC_ERR_OK);

    return value;
}

float read_float_reg(CPUState &state, size_t index) {
    assert(index >= 0);

    DoubleReg value;

    const int single_index = static_cast<int>(index / 2);
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_D0 + single_index, &value);
    assert(err == UC_ERR_OK);
    return value.f[index % 2];
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

uint32_t read_fpscr(CPUState &state) {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_FPSCR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

uint32_t read_cpsr(CPUState &state) {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_CPSR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

uint32_t read_TPIDRURO(CPUState &state) {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(state.uc.get(), UC_ARM_REG_C13_C0_3, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void write_reg(CPUState &state, size_t index, uint32_t value) {
    assert(index >= 0);

    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_R0 + static_cast<int>(index), &value);
    assert(err == UC_ERR_OK);
}

void write_float_reg(CPUState &state, size_t index, float value) {
    assert(index >= 0);

    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_R0 + static_cast<int>(index), &value);
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

void write_fpscr(CPUState &state, uint32_t value) {
    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_FPSCR, &value);
    assert(err == UC_ERR_OK);
}

void write_cpsr(CPUState &state, uint32_t value) {
    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_CPSR, &value);
    assert(err == UC_ERR_OK);
}

void write_TPIDRURO(CPUState &state, uint32_t value) {
    const uc_err err = uc_reg_write(state.uc.get(), UC_ARM_REG_C13_C0_3, &value);
    assert(err == UC_ERR_OK);
}

bool hit_breakpoint(CPUState &state) {
    return state.did_break;
}

void trigger_breakpoint(CPUState &state) {
    stop(state);
    state.did_break = true;
}

std::string disassemble(CPUState &state, uint64_t at, bool thumb, uint16_t *insn_size) {
    MemState &mem = *state.mem;
    const uint8_t *const code = Ptr<const uint8_t>(static_cast<Address>(at)).get(mem);
    const size_t buffer_size = GB(4) - at;
    return disassemble(state.disasm, code, buffer_size, at, thumb, insn_size);
}

std::string disassemble(CPUState &state, uint64_t at, uint16_t *insn_size) {
    const bool thumb = is_thumb_mode(state.uc.get());
    return disassemble(state, at, thumb, insn_size);
}

void log_code_add(CPUState &state) {
    uc_hook hh = 0;
    const uc_err err = uc_hook_add(state.uc.get(), &hh, UC_HOOK_CODE, reinterpret_cast<void *>(&code_hook), &state, 1, 0);

    assert(err == UC_ERR_OK);
}

void log_mem_add(CPUState &state) {
    uc_hook hh = 0;
    uc_err err = uc_hook_add(state.uc.get(), &hh, UC_HOOK_MEM_READ, reinterpret_cast<void *>(&read_hook), &state, 1, 0);
    assert(err == UC_ERR_OK);

    err = uc_hook_add(state.uc.get(), &hh, UC_HOOK_MEM_WRITE, reinterpret_cast<void *>(&write_hook), &state, 1, 0);
    assert(err == UC_ERR_OK);
}

static void delete_cpu_context(CPUContext *ctx) {
    delete ctx;
}

CPUContextPtr init_cpu_context() {
    CPUContextPtr ctx(new CPUContext(), delete_cpu_context);
    return ctx;
}

void save_context(CPUState &state, CPUContext &ctx) {

    for (auto i = 0; i < 16; i++) {
        ctx.cpu_registers[i] = read_reg(state, i);
    }

    ctx.pc = read_pc(state);
    ctx.lr = read_lr(state);
    ctx.sp = read_sp(state);

}

void load_context(CPUState &state, CPUContext &ctx) {

    bool thumb_mode = is_thumb_mode(state.uc.get());
    if (thumb_mode) {
        ctx.pc |= 1;
    }

    for (auto i = 0; i < 16; i++) {
        write_reg(state, i, ctx.cpu_registers[i]);
    }

    write_pc(state, ctx.pc);
    write_lr(state, ctx.lr);
    write_sp(state, ctx.sp);
}
