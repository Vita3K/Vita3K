#include <cpu/functions.h>
#include <cpu/impl/unicorn_cpu.h>
#include <mem/ptr.h>
#include <util/log.h>

#include <cassert>
#include <cpu/disasm/functions.h>

#include <util/string_utils.h>

constexpr bool TRACE_RETURN_VALUES = true;
constexpr bool LOG_REGISTERS = false;

static inline void func_trace(CPUState &state) {
    if (TRACE_RETURN_VALUES)
        if (is_returning(state.disasm))
            LOG_TRACE("Returning, r0: {}", log_hex(read_reg(state, 0)));
}

void UnicornCPU::code_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    UnicornCPU &state = *static_cast<UnicornCPU *>(user_data);
    std::string disassembly = disassemble(*state.parent, address);
    if (LOG_REGISTERS) {
        for (int i = 0; i < 12; i++) {
            auto reg_name = fmt::format("r{}", i);
            auto reg_name_with_value = fmt::format("{}({})", reg_name, log_hex(state.get_reg(i)));
            string_utils::replace(disassembly, reg_name, reg_name_with_value);
        }

        string_utils::replace(disassembly, "lr", fmt::format("lr({})", log_hex(state.get_lr())));
        string_utils::replace(disassembly, "sp", fmt::format("sp({})", log_hex(state.get_sp())));
    }

    LOG_TRACE("{} ({}): {} {}", log_hex((uint64_t)uc), state.parent->thread_id, log_hex(address), disassembly);

    func_trace(*state.parent);
}

void UnicornCPU::read_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    assert(value == 0);

    UnicornCPU &state = *static_cast<UnicornCPU *>(user_data);
    MemState &mem = *state.parent->mem;
    auto start = state.parent->protocol->get_watch_memory_addr(address);
    if (start) {
        memcpy(&value, Ptr<const void>(static_cast<Address>(address)).get(mem), size);
        state.log_memory_access(uc, "Read", start, size, value, mem, *state.parent, address - start);
    }
}

void UnicornCPU::write_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    UnicornCPU &state = *static_cast<UnicornCPU *>(user_data);
    MemState &mem = *state.parent->mem;
    auto start = state.parent->protocol->get_watch_memory_addr(address);
    if (start) {
        MemState &mem = *state.parent->mem;
        state.log_memory_access(uc, "Write", start, size, value, mem, *state.parent, address - start);
    }
}

void UnicornCPU::log_memory_access(uc_engine *uc, const char *type, Address address, int size, int64_t value, MemState &mem, CPUState &cpu, Address offset) {
    const char *const name = mem_name(address, mem);
    auto pc = get_pc();
    LOG_TRACE("{} ({}): {} {} bytes, address {} + {} ({}, {}), value {} at {}", log_hex((uint64_t)uc), cpu.thread_id, type, size, log_hex(address), log_hex(offset), log_hex(address + offset), name, log_hex(value), log_hex(pc));
}

constexpr uint32_t INT_SVC = 2;
constexpr uint32_t INT_BKPT = 7;

void UnicornCPU::intr_hook(uc_engine *uc, uint32_t intno, void *user_data) {
    assert(intno == INT_SVC || intno == INT_BKPT);
    UnicornCPU &state = *static_cast<UnicornCPU *>(user_data);
    uint32_t pc = state.get_pc();
    state.is_inside_intr_hook = true;
    if (intno == INT_SVC) {
        assert(!state.is_thumb_mode());
        const Address svc_address = pc - 4;
        uint32_t svc_instruction = 0;
        const auto err = uc_mem_read(uc, svc_address, &svc_instruction, sizeof(svc_instruction));
        assert(err == UC_ERR_OK);
        const uint32_t imm = state.is_thumb_mode() ? (svc_instruction & 0xff0000) >> 16 : svc_instruction & 0xffffff;
        state.parent->protocol->call_svc(*state.parent, imm, pc, get_thread_id(*state.parent));
    } else if (intno == INT_BKPT) {
        state.stop();
        state.did_break = true;
    }
    state.is_inside_intr_hook = false;
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

void UnicornCPU::log_error_details(uc_err code) {
    // I don't especially want the time logged for every line, but I also want it to print to the log file...
    LOG_ERROR("Unicorn error {}. {}", log_hex(code), uc_strerror(code));

    uint32_t pc = get_pc();
    uint32_t sp = get_sp();
    uint32_t lr = get_lr();
    uint32_t registers[13];
    for (size_t a = 0; a < 13; a++)
        registers[a] = get_reg(a);

    LOG_ERROR("PC: 0x{:0>8x},   SP: 0x{:0>8x},   LR: 0x{:0>8x}", pc, sp, lr);
    for (int a = 0; a < 6; a++) {
        LOG_ERROR("r{: <2}: 0x{:0>8x}   r{: <2}: 0x{:0>8x}", a, registers[a], a + 6, registers[a + 6]);
    }
    LOG_ERROR("r12: 0x{:0>8x}", registers[12]);
    LOG_ERROR("Executing: {}", disassemble(*this->parent, pc));
}

UnicornCPU::UnicornCPU(CPUState *state)
    : parent(state) {
    uc_engine *temp_uc = nullptr;
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &temp_uc);
    assert(err == UC_ERR_OK);

    uc = UnicornPtr(temp_uc, uc_close);
    temp_uc = nullptr;

    uc_hook hh = 0;

    err = uc_hook_add(uc.get(), &hh, UC_HOOK_INTR, reinterpret_cast<void *>(&intr_hook), this, 1, 0);
    assert(err == UC_ERR_OK);

    // Don't map the null page into unicorn so that unicorn returns access error instead of
    // crashing the whole emulator on invalid access
    err = uc_mem_map_ptr(uc.get(), state->mem->page_size, GB(4) - state->mem->page_size, UC_PROT_ALL, &state->mem->memory[state->mem->page_size]);
    assert(err == UC_ERR_OK);

    enable_vfp_fpu(uc.get());
}

int UnicornCPU::execute_instructions_no_check(int num) {
    std::uint32_t pc = get_pc();
    bool thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    uc_err err = uc_emu_start(uc.get(), pc, 1ULL << 63, 0, num);

    if (err != UC_ERR_OK) {
        log_error_details(err);
        return -1;
    }

    return 0;
}

int UnicornCPU::run() {
    uint32_t pc = get_pc();
    bool thumb_mode = is_thumb_mode();
    did_break = false;

    pc = get_pc();
    if (thumb_mode) {
        pc |= 1;
    }

    uc_err err = uc_emu_start(uc.get(), pc, 0, 0, 0);
    if (err != UC_ERR_OK) {
        log_error_details(err);
        return -1;
    }

    pc = get_pc();
    thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    return parent->halt_instruction_pc <= pc && pc <= parent->halt_instruction_pc + 4;
}

int UnicornCPU::step() {
    uint32_t pc = get_pc();
    bool thumb_mode = is_thumb_mode();

    did_break = false;

    pc = get_pc();
    if (thumb_mode) {
        pc |= 1;
    }

    uc_err err = uc_emu_start(uc.get(), pc, 0, 0, 1);

    if (err != UC_ERR_OK) {
        log_error_details(err);
        return -1;
    }
    pc = get_pc();
    thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    return parent->halt_instruction_pc <= pc && pc <= parent->halt_instruction_pc + 4;
}

void UnicornCPU::stop() {
    const uc_err err = uc_emu_stop(uc.get());
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_reg(uint8_t idx) {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_R0 + static_cast<int>(idx), &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_reg(uint8_t idx, uint32_t val) {
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_R0 + static_cast<int>(idx), &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_sp() {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_SP, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_sp(uint32_t val) {
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_SP, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_pc() {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_PC, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_pc(uint32_t val) {
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_PC, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_lr() {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_LR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_lr(uint32_t val) {
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_LR, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_cpsr() {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_CPSR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_cpsr(uint32_t val) {
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_CPSR, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_tpidruro() {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_C13_C0_3, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_tpidruro(uint32_t val) {
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_C13_C0_3, &val);
    assert(err == UC_ERR_OK);
}

uint32_t UnicornCPU::get_fpscr() {
    uint32_t value = 0;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_FPSCR, &value);
    assert(err == UC_ERR_OK);

    return value;
}

void UnicornCPU::set_fpscr(uint32_t val) {
    const uc_err err = uc_reg_write(uc.get(), UC_ARM_REG_FPSCR, &val);
    assert(err == UC_ERR_OK);
}

float UnicornCPU::get_float_reg(uint8_t idx) {
    DoubleReg value;

    const int single_index = static_cast<int>(idx / 2);
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_D0 + single_index, &value);
    assert(err == UC_ERR_OK);
    return value.f[idx % 2];
}

void UnicornCPU::set_float_reg(uint8_t idx, float val) {
    DoubleReg value;

    const int single_index = static_cast<int>(idx / 2);
    uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_D0 + single_index, &value);
    assert(err == UC_ERR_OK);
    value.f[idx % 2] = val;

    err = uc_reg_write(uc.get(), UC_ARM_REG_D0 + single_index, &value);
    assert(err == UC_ERR_OK);
}

bool UnicornCPU::is_thumb_mode() {
    size_t mode = 0;
    const uc_err err = uc_query(uc.get(), UC_QUERY_MODE, &mode);
    assert(err == UC_ERR_OK);

    return mode & UC_MODE_THUMB;
}

CPUContext UnicornCPU::save_context() {
    CPUContext ctx;
    for (size_t i = 0; i < 16; i++) {
        ctx.cpu_registers[i] = get_reg(i);
    }
    ctx.cpu_registers[13] = get_sp();
    ctx.cpu_registers[14] = get_lr();
    ctx.set_pc(is_thumb_mode() ? get_pc() | 1 : get_pc());

    for (size_t i = 0; i < ctx.fpu_registers.size(); i++) {
        ctx.fpu_registers[i] = get_float_reg(i);
    }

    // Unicorn doesn't like tweaking cpsr
    // ctx.cpsr = get_cpsr();
    // ctx.fpscr = get_fpscr();

    return ctx;
}

void UnicornCPU::load_context(CPUContext ctx) {
    for (size_t i = 0; i < ctx.fpu_registers.size(); i++) {
        set_float_reg(i, ctx.fpu_registers[i]);
    }

    // Unicorn doesn't like tweaking cpsr
    // set_cpsr(ctx.cpsr);
    // set_fpscr(ctx.fpscr);

    for (size_t i = 0; i < 16; i++) {
        set_reg(i, ctx.cpu_registers[i]);
    }
    set_sp(ctx.get_sp());
    set_lr(ctx.get_lr());
    set_pc(ctx.thumb() ? ctx.get_pc() | 1 : ctx.get_pc());
}

bool UnicornCPU::hit_breakpoint() {
    return did_break;
}

void UnicornCPU::trigger_breakpoint() {
    stop();
    did_break = true;
}

void UnicornCPU::set_log_code(bool log) {
    if (get_log_code() == log) {
        return;
    }
    if (log) {
        const uc_err err = uc_hook_add(uc.get(), &code_hook_handle, UC_HOOK_CODE, reinterpret_cast<void *>(&code_hook), this, 1, 0);

        assert(err == UC_ERR_OK);
    } else {
        auto err = uc_hook_del(uc.get(), code_hook_handle);
        assert(err == UC_ERR_OK);
        code_hook_handle = 0;
    }
}

void UnicornCPU::set_log_mem(bool log) {
    if (get_log_mem() == log) {
        return;
    }
    if (log) {
        uc_err err = uc_hook_add(uc.get(), &memory_read_hook_handle, UC_HOOK_MEM_READ, reinterpret_cast<void *>(&read_hook), this, 1, 0);
        assert(err == UC_ERR_OK);

        err = uc_hook_add(uc.get(), &memory_write_hook_handle, UC_HOOK_MEM_WRITE, reinterpret_cast<void *>(&write_hook), this, 1, 0);
        assert(err == UC_ERR_OK);
    } else {
        auto err = uc_hook_del(uc.get(), memory_read_hook_handle);
        assert(err == UC_ERR_OK);
        memory_read_hook_handle = 0;

        err = uc_hook_del(uc.get(), memory_write_hook_handle);
        assert(err == UC_ERR_OK);
        memory_write_hook_handle = 0;
    }
}

bool UnicornCPU::get_log_code() {
    return code_hook_handle != 0;
}

bool UnicornCPU::get_log_mem() {
    return memory_read_hook_handle != 0 && memory_write_hook_handle != 0;
}
