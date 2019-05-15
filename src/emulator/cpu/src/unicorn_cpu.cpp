#include <cpu/functions.h>
#include <cpu/unicorn_cpu.h>
#include <mem/ptr.h>
#include <util/log.h>

#include <cassert>
#include <disasm/functions.h>

bool thumb_mode(uc_engine *uc) {
    size_t mode = 0;
    auto err = uc_query(uc, UC_QUERY_MODE, &mode);

    if (err != UC_ERR_OK) {
        LOG_INFO("Query mode failed");
        return false;
    }

    return mode & UC_MODE_THUMB;
}

// Log code for specified threads (arg to create_thread)
static const bool LOG_CODE = true;
// Log code run on all threads
static bool LOG_CODE_ALL = false;
static bool LOG_MEM_ACCESS = false;

static void delete_cpu_state(CPUState *state) {
    delete state;
}

static void code_hook(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
    CPUState &state = *static_cast<CPUState *>(user_data);
    const MemState &mem = *state.mem;
    const uint8_t *const code = Ptr<const uint8_t>(static_cast<Address>(address)).get(mem);
    const size_t buffer_size = GB(4) - address;
    const bool thumb = thumb_mode(uc);
    const std::string disassembly = disassemble(state.disasm, code, buffer_size, address, thumb);
    LOG_TRACE("{} {}", log_hex(address), disassembly);
}

static void log_memory_access(const char *type, Address address, int size, int64_t value, MemState &mem) {
    const char *const name = mem_name(address, mem);
    LOG_TRACE("{} {} bytes, address {} ( {} ), value {}", type, size, address, name, log_hex(value));
}

static void read_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    assert(value == 0);

    const CPUState &state = *static_cast<const CPUState *>(user_data);
    MemState &mem = *state.mem;
    memcpy(&value, Ptr<const void>(static_cast<Address>(address)).get(mem), size);
    log_memory_access("Read", static_cast<Address>(address), size, value, mem);
}

static void write_hook(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
    const CPUState &state = *static_cast<const CPUState *>(user_data);
    MemState &mem = *state.mem;
    log_memory_access("Write", static_cast<Address>(address), size, value, mem);
}

constexpr uint32_t INT_SVC = 2;
constexpr uint32_t INT_BKPT = 7;

static void intr_hook(uc_engine *uc, uint32_t intno, void *user_data) {
    assert(intno == 2);

    CPUState &state = *static_cast<CPUState *>(user_data);

    uint32_t pc = 0;
    uc_err err = uc_reg_read(uc, UC_ARM_REG_PC, &pc);
    assert(err == UC_ERR_OK);

    switch (intno) {
    case INT_SVC:
        if (thumb_mode(uc)) {
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

UnicornCPU::UnicornCPU(CPUState *state, Address pc, Address sp, bool log_code) {
    uc_engine *temp_uc = nullptr;
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &temp_uc);
    assert(err == UC_ERR_OK);

    uc = UnicornPtr(temp_uc, uc_free);

    temp_uc = nullptr;
    uc_hook hh = 0;

    err = uc_hook_add(uc.get(), &hh, UC_HOOK_INTR, reinterpret_cast<void *>(&intr_hook), state, 1, 0);
    assert(err == UC_ERR_OK);

    if ((log_code && LOG_CODE) || LOG_CODE_ALL) {
        uc_hook hh = 0;
        const uc_err err = uc_hook_add(uc.get(), &hh, UC_HOOK_CODE, reinterpret_cast<void *>(&code_hook), &state, 1, 0);

        assert(err == UC_ERR_OK);
    }

    if (LOG_MEM_ACCESS) {
        uc_hook hh = 0;
        uc_err err = uc_hook_add(uc.get(), &hh, UC_HOOK_MEM_READ, reinterpret_cast<void *>(&read_hook), &state, 1, 0);
        assert(err == UC_ERR_OK);

        err = uc_hook_add(uc.get(), &hh, UC_HOOK_MEM_WRITE, reinterpret_cast<void *>(&write_hook), &state, 1, 0);
        assert(err == UC_ERR_OK);
    }

    err = uc_reg_write(uc.get(), UC_ARM_REG_SP, &sp);
    assert(err == UC_ERR_OK);

    err = uc_mem_map_ptr(uc.get(), 0, GB(4), UC_PROT_ALL, &(state->mem->memory[0]));
    assert(err == UC_ERR_OK);

    err = uc_reg_write(uc.get(), UC_ARM_REG_PC, &pc);

    assert(err == UC_ERR_OK);

    err = uc_reg_write(uc.get(), UC_ARM_REG_LR, &pc);

    assert(err == UC_ERR_OK);

    ep = pc;

    enable_vfp_fpu(uc.get());
}

/*! \brief Execute a amount of instructions. */
int UnicornCPU::execute_instructions(int num) {
    std::uint32_t pc = get_pc();
    bool thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }
    uc_err err = uc_emu_start(uc.get(), pc, 0, 0, 1);

    pc = get_pc();
    thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }
    err = uc_emu_start(uc.get(), pc, ep & 0xFFFFFFFE, 0, (num == -1) ? 0 : num);

    if (err != UC_ERR_OK) {
        std::uint32_t error_pc = get_pc();
        uint32_t lr = get_lr();
        LOG_CRITICAL("Unicorn error {} at: start PC: {} error PC {} LR: {}", log_hex(err), log_hex(pc), log_hex(error_pc), log_hex(lr));
        return -1;
    }

    pc = get_pc();
    thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    if (pc == ep) {
        return 1;
    }

    return 0;
}

int UnicornCPU::execute_instructions_no_check(int num) {
    std::uint32_t pc = get_pc();
    bool thumb_mode = is_thumb_mode();
    if (thumb_mode) {
        pc |= 1;
    }

    uc_err err = uc_emu_start(uc.get(), pc, 1ULL << 63, 0, num);

    if (err != UC_ERR_OK) {
        std::uint32_t error_pc = get_pc();
        uint32_t lr = get_lr();
        LOG_CRITICAL("Unicorn error {} at: start PC: {} error PC {} LR: {}", log_hex(err), log_hex(get_pc()), log_hex(error_pc), log_hex(lr));
        return -1;
    }

    return 0;
}

/*! Run the CPU */
int UnicornCPU::run() {
    return execute_instructions(-1);
}

int UnicornCPU::step() {
    return execute_instructions(1);
}

/*! Stop the CPU */
void UnicornCPU::stop() {
    uc_emu_stop(uc.get());
}

/*! Get a specific ARM Rx register. Range from r0 to r15 */
uint32_t UnicornCPU::get_reg(uint8_t idx) {
    uint32_t val = 0;
    auto treg = UC_ARM_REG_R0 + static_cast<uint8_t>(idx);
    auto err = uc_reg_read(uc.get(), treg, &val);

    if (err != UC_ERR_OK) {
        LOG_ERROR("Failed to get ARM CPU registers.");
    }

    return val;
}

/*! Get the stack pointer */
uint32_t UnicornCPU::get_sp() {
    uint32_t ret = 0;
    auto err = uc_reg_read(uc.get(), UC_ARM_REG_SP, &ret);

    if (err != UC_ERR_OK) {
        LOG_WARN("Failed to read ARM CPU SP!");
    }

    return ret;
}

/*! Get the program counter */
uint32_t UnicornCPU::get_pc() {
    uint32_t ret = 0;
    auto err = uc_reg_read(uc.get(), UC_ARM_REG_PC, &ret);

    if (err != UC_ERR_OK) {
        LOG_WARN("Failed to read ARM CPU PC!");
    }

    return ret;
}

/*! Set a Rx register */
void UnicornCPU::set_reg(uint8_t idx, uint32_t val) {
    auto treg = UC_ARM_REG_R0 + idx;
    auto err = uc_reg_write(uc.get(), treg, &val);

    if (err != UC_ERR_OK) {
        LOG_ERROR("Failed to set ARM CPU registers.");
    }
}

void UnicornCPU::set_cpsr(uint32_t val) {
    auto err = uc_reg_write(uc.get(), UC_ARM_REG_CPSR, &val);

    if (err != UC_ERR_OK) {
        LOG_ERROR("Writing cpsr failed!");
    }
}

/*! Set program counter */
void UnicornCPU::set_pc(uint32_t val) {
    auto err = uc_reg_write(uc.get(), UC_ARM_REG_PC, &val);

    if (err != UC_ERR_OK) {
        LOG_WARN("ARM PC failed to be set!");
    }
}

/*! Set LR */
void UnicornCPU::set_lr(uint32_t val) {
    auto err = uc_reg_write(uc.get(), UC_ARM_REG_LR, &val);

    if (err != UC_ERR_OK) {
        LOG_WARN("ARM PC failed to be set!");
    }
}

uint32_t UnicornCPU::get_lr() {
    uint32_t ret = 0;
    auto err = uc_reg_read(uc.get(), UC_ARM_REG_LR, &ret);

    if (err != UC_ERR_OK) {
        LOG_WARN("Failed to read ARM CPU LR!");
    }

    return ret;
}

/*! Set stack pointer */
void UnicornCPU::set_sp(uint32_t val) {
    auto err = uc_reg_write(uc.get(), UC_ARM_REG_SP, &val);

    if (err != UC_ERR_OK) {
        LOG_WARN("ARM SP failed to be set!");
    }
}

uint32_t UnicornCPU::get_cpsr() {
    uint32_t cpsr = 0;
    auto err = uc_reg_read(uc.get(), UC_ARM_REG_CPSR, &cpsr);

    return cpsr;
}

uint32_t UnicornCPU::get_fpscr() {
    uint32_t cpsr = 0;
    auto err = uc_reg_read(uc.get(), UC_ARM_REG_FPSCR, &cpsr);

    return cpsr;
}

bool UnicornCPU::is_thumb_mode() {
    return thumb_mode(uc.get());
}

float UnicornCPU::get_float_reg(uint8_t idx) {
    DoubleReg value;

    int single_index = idx / 2;
    const uc_err err = uc_reg_read(uc.get(), UC_ARM_REG_D0 + single_index, &value);
    assert(err == UC_ERR_OK);

    return value.f[idx % 2];
}

UnicornCPU::ThreadContext UnicornCPU::save_context() {
    ThreadContext ctx;

    for (auto i = 0; i < ctx.cpu_registers.size(); i++) {
        ctx.cpu_registers[i] = get_reg(i);
    }

    for (auto i = 0; i < ctx.fpu_registers.size(); i++) {
        uc_reg_write(uc.get(), UC_ARM_REG_S0 + i, &ctx.fpu_registers[i]);
    }

    ctx.pc = get_pc();
    ctx.sp = get_sp();
    ctx.cpsr = get_cpsr();

    if (ctx.cpsr & 0x20) {
        ctx.pc |= 1;
    }

    return ctx;
}

void UnicornCPU::load_context(UnicornCPU::ThreadContext ctx) {
    for (auto i = 0; i < ctx.cpu_registers.size(); i++) {
        set_reg(i, ctx.cpu_registers[i]);
    }

    for (auto i = 0; i < ctx.fpu_registers.size(); i++) {
        set_reg(i, ctx.fpu_registers[i]);
    }

    set_sp(ctx.sp);
    set_lr(ctx.cpu_registers[14]);
    set_pc(ctx.pc);
    set_cpsr(ctx.cpsr);
}
