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
#include <util/types.h>

#include <unicorn/unicorn.h>

#include <cassert>
#include <cstring>

#include <spdlog/fmt/fmt.h>
#include <util/string_utils.h>

typedef std::unique_ptr<uc_struct, std::function<void(uc_struct *)>> UnicornPtr;

constexpr bool LOG_REGISTERS = false;

union DoubleReg {
    double d;
    float f[2];
};

struct CPUState {
    SceUID thread_id;
    MemState *mem = nullptr;
    CallSVC call_svc;
    ResolveNIDName resolve_nid_name;
    DisasmState disasm;
    GetWatchMemoryAddr get_watch_memory_addr;
    UnicornPtr uc;
    uc_hook memory_read_hook = 0;
    uc_hook memory_write_hook = 0;
    uc_hook code_hook = 0;

    bool did_break = false;
    bool did_inject = false;

    std::vector<ModuleRegion> module_regions;
    std::stack<StackFrame> stack_frames;
};

constexpr bool TRACE_RETURN_VALUES = true;

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

std::stack<StackFrame> get_stack_frames(CPUState &state) {
    return state.stack_frames;
}

void push_stack_frame(CPUState &state, StackFrame sf) {
    state.stack_frames.push(sf);
}

static void stack_trace_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    CPUState &state = *static_cast<CPUState *>(user_data);
    auto thumb_mode = is_thumb_mode(uc);
    if (thumb_mode) {
        uint16_t ins = 0;
        uc_err err = uc_mem_read(uc, address, &ins, sizeof(ins));
        assert(err == UC_ERR_OK);
        if ((ins & 0xff00) == 0xB400 || (ins & 0xff00) == 0xB500 || ins == 0xE92D) {
            state.stack_frames.push({ static_cast<uint32_t>(address), read_sp(state) });
        }
        if ((ins & 0xff00) == 0xBC00 || (ins & 0xff00) == 0xBD00 || ins == 0xE8BD) {
            state.stack_frames.pop();
        }
    } else {
        uint16_t ins = 0;
        uc_err err = uc_mem_read(uc, address + 2, &ins, sizeof(ins));
        assert(err == UC_ERR_OK);
        if (ins == 0xE92D) {
            state.stack_frames.push({ static_cast<uint32_t>(address), read_sp(state) });
        }
        if (ins == 0xE8BD) {
            state.stack_frames.pop();
        }
    }
}

static void code_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    CPUState &state = *static_cast<CPUState *>(user_data);
    std::string disassembly = disassemble(state, address);
    if (LOG_REGISTERS) {
        for (int i = 0; i < 12; i++) {
            auto reg_name = fmt::format("r{}", i);
            auto reg_name_with_value = fmt::format("{}({})", reg_name, log_hex(read_reg(state, i)));
            string_utils::replace(disassembly, reg_name, reg_name_with_value);
        }

        string_utils::replace(disassembly, "lr", fmt::format("lr({})", log_hex(read_lr(state))));
        string_utils::replace(disassembly, "sp", fmt::format("sp({})", log_hex(read_sp(state))));
    }

    auto name = state.resolve_nid_name(address);
    if (name != "") {
        LOG_TRACE("{} ({}): {} {} entering export function {}", log_hex((uint64_t)uc), state.thread_id, log_hex(address), disassembly, name);
    } else {
        LOG_TRACE("{} ({}): {} {}", log_hex((uint64_t)uc), state.thread_id, log_hex(address), disassembly);
    }

    func_trace(state);

    log_stack_frames(state);
}

static void log_memory_access(uc_engine *uc, const char *type, Address address, int size, int64_t value, MemState &mem, CPUState &cpu, Address offset) {
    const char *const name = mem_name(address, mem);
    auto pc = read_pc(cpu);
    LOG_TRACE("{} ({}): {} {} bytes, address {} + {} ({}, {}), value {} at {}", log_hex((uint64_t)uc), cpu.thread_id, type, size, log_hex(address), log_hex(offset), log_hex(address + offset), name, log_hex(value), log_hex(pc));
}

static void read_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    assert(value == 0);

    CPUState &state = *static_cast<CPUState *>(user_data);
    MemState &mem = *state.mem;
    auto start = state.get_watch_memory_addr(address);
    if (start) {
        memcpy(&value, Ptr<const void>(static_cast<Address>(address)).get(mem), size);
        log_memory_access(uc, "Read", start, size, value, mem, state, address - start);
    }
}

static void write_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    CPUState &state = *static_cast<CPUState *>(user_data);
    MemState &mem = *state.mem;
    auto start = state.get_watch_memory_addr(address);
    if (start) {
        MemState &mem = *state.mem;
        log_memory_access(uc, "Write", start, size, value, mem, state, address - start);
    }
}

constexpr uint32_t INT_SVC = 2;
constexpr uint32_t INT_BKPT = 7;

static void intr_hook(uc_engine *uc, uint32_t intno, void *user_data) {
    assert(intno == INT_SVC || intno == INT_BKPT);

    CPUState &state = *static_cast<CPUState *>(user_data);

    uint32_t pc = read_pc(state);
    auto thumb_mode = is_thumb_mode(uc);
    if (intno == INT_SVC) {
        if (thumb_mode) {
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
    } else if (intno == INT_BKPT) {
        auto &bks = state.mem->breakpoints;
        if (bks.find(pc) != bks.end()) {
            auto bk = bks[pc];
            stop(state);
            if (bk.gdb) {
                state.did_break = true;
            } else {
                state.did_inject = true;
                if (bk.callback) {
                    bk.callback(state, *state.mem);
                }
            }
        }
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

    LOG_ERROR("PC: 0x{:0>8x},   SP: 0x{:0>8x},   LR: 0x{:0>8x}", pc, sp, lr);
    for (int a = 0; a < 6; a++) {
        LOG_ERROR("r{: <2}: 0x{:0>8x}   r{: <2}: 0x{:0>8x}", a, registers[a], a + 6, registers[a + 6]);
    }
    LOG_ERROR("r12: 0x{:0>8x}", registers[12]);
    LOG_ERROR("Executing: {}", disassemble(state, pc));
    log_stack_frames(state);
}

std::unique_ptr<ModuleRegion> get_region(CPUState &state, Address addr) {
    for (const auto &region : state.module_regions) {
        if (region.start <= addr && addr < region.start + region.size) {
            return std::make_unique<ModuleRegion>(region);
        }
    }
    return nullptr;
}

void log_stack_frames(CPUState &cpu) {
    auto sfs = get_stack_frames(cpu);
    LOG_INFO("stack information");
    int i = 0;
    while (!sfs.empty() && i < 50) {
        i++;
        auto sf = sfs.top();
        sfs.pop();

        auto region = get_region(cpu, sf.addr);
        assert(region);
        auto vaddr = sf.addr - region->start + region->vaddr;
        LOG_INFO("---------");
        LOG_INFO("module: {}", region->name);
        LOG_INFO("vaddr: {}", log_hex(vaddr));
        LOG_INFO("addr: {}", log_hex(sf.addr));
        LOG_INFO("fp: {}", sf.sp);
    }
}

CPUStatePtr init_cpu(SceUID thread_id, Address pc, Address sp, MemState &mem, CPUDepInject &inject) {
    CPUStatePtr state(new CPUState(), delete_cpu_state);
    state->mem = &mem;
    state->call_svc = inject.call_svc;
    state->resolve_nid_name = inject.resolve_nid_name;
    state->get_watch_memory_addr = inject.get_watch_memory_addr;
    state->thread_id = thread_id;
    state->module_regions = inject.module_regions;
    if (!init(state->disasm)) {
        return CPUStatePtr();
    }

    uc_engine *temp_uc = nullptr;
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &temp_uc);
    assert(err == UC_ERR_OK);

    state->uc = UnicornPtr(temp_uc, uc_close);
    temp_uc = nullptr;

    uc_hook hh = 0;

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

    if (inject.trace_stack) {
        err = uc_hook_add(state->uc.get(), &hh, UC_HOOK_CODE, reinterpret_cast<void *>(&stack_trace_hook), state.get(), 1, 0);
        assert(err == UC_ERR_OK);
    }

    return state;
}

int _run_after_injected(CPUState &state, uint32_t pc, bool thumb_mode) {
    // run the original instruction before injecting bkpt
    // then, inject the bkpt again

    assert(state.mem->breakpoints.find(pc) != state.mem->breakpoints.end());

    unsigned char original[4];
    auto bk = state.mem->breakpoints[pc];

    size_t size = thumb_mode ? sizeof(unsigned char[2]) : sizeof(unsigned char[4]);

    std::memcpy(original, &state.mem->memory[pc], size);
    std::memcpy(&state.mem->memory[pc], bk.data, size);

    uc_err err;
    if (thumb_mode) {
        err = uc_emu_start(state.uc.get(), pc | 1, 0, 0, 1);
    } else {
        err = uc_emu_start(state.uc.get(), pc, 0, 0, 1);
    }

    std::memcpy(&state.mem->memory[pc], original, size);
    if (err != UC_ERR_OK) {
        log_error_details(state, err);
#ifdef USE_GDBSTUB
        trigger_breakpoint(state);
        return 0;
#else
        return -1;
#endif
    }
    return 0;
}

int run(CPUState &state, bool callback, Address entry_point) {
    uint32_t pc = read_pc(state);
    bool thumb_mode = is_thumb_mode(state.uc.get());
    state.did_break = false;
    if (state.did_inject && _run_after_injected(state, pc, thumb_mode)) {
        return -1;
    }
    state.did_inject = false;

    pc = read_pc(state);
    if (thumb_mode) {
        pc |= 1;
    }
    if (callback) {
        uc_reg_write(state.uc.get(), UC_ARM_REG_LR, &entry_point);
    }

    uc_err err = uc_emu_start(state.uc.get(), pc, 0, 0, 1);
    pc = read_pc(state);
    thumb_mode = is_thumb_mode(state.uc.get());
    if (thumb_mode) {
        pc |= 1;
    }

    err = uc_emu_start(state.uc.get(), pc, entry_point & 0xfffffffe, 0, 0);
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

    return pc == entry_point;
}

int step(CPUState &state, bool callback, Address entry_point) {
    uint32_t pc = read_pc(state);
    bool thumb_mode = is_thumb_mode(state.uc.get());

    state.did_break = false;
    if (state.did_inject && _run_after_injected(state, pc, thumb_mode)) {
        return -1;
    }
    state.did_inject = false;

    pc = read_pc(state);
    if (thumb_mode) {
        pc |= 1;
    }
    if (callback) {
        uc_reg_write(state.uc.get(), UC_ARM_REG_LR, &entry_point);
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

    return pc == entry_point;
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

uint32_t read_tpidruro(CPUState &state) {
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

void write_tpidruro(CPUState &state, uint32_t value) {
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
    const uc_err err = uc_hook_add(state.uc.get(), &state.code_hook, UC_HOOK_CODE, reinterpret_cast<void *>(&code_hook), &state, 1, 0);

    assert(err == UC_ERR_OK);
}

void log_code_remove(CPUState &state) {
    auto err = uc_hook_del(state.uc.get(), state.code_hook);
    assert(err == UC_ERR_OK);
    state.code_hook = 0;
}

void log_mem_add(CPUState &state) {
    uc_err err = uc_hook_add(state.uc.get(), &state.memory_read_hook, UC_HOOK_MEM_READ, reinterpret_cast<void *>(&read_hook), &state, 1, 0);
    assert(err == UC_ERR_OK);

    err = uc_hook_add(state.uc.get(), &state.memory_write_hook, UC_HOOK_MEM_WRITE, reinterpret_cast<void *>(&write_hook), &state, 1, 0);
    assert(err == UC_ERR_OK);
}

void log_mem_remove(CPUState &state) {
    auto err = uc_hook_del(state.uc.get(), state.memory_read_hook);
    assert(err == UC_ERR_OK);
    state.memory_read_hook = 0;

    err = uc_hook_del(state.uc.get(), state.memory_write_hook);
    assert(err == UC_ERR_OK);
    state.memory_write_hook = 0;
}

bool log_code_exists(CPUState &state) {
    return state.code_hook != 0;
}

bool log_mem_exists(CPUState &state) {
    return state.memory_read_hook != 0 && state.memory_write_hook != 0;
}

static void delete_cpu_context(CPUContext *ctx) {
    delete ctx;
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
