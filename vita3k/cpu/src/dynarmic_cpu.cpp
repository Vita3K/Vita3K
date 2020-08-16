#include <cpu/dynarmic_cpu.h>
#include <cpu/interface.h>
#include <cpu/state.h>
#include <util/log.h>
#include <set>
#include <dynarmic/A32/context.h>

#include <mem/ptr.h>

class ArmDynarmicCallback : public Dynarmic::A32::UserCallbacks {
    friend class DynarmicCPU;

    CPUState *parent;
    DynarmicCPU *cpu;
    uint64_t interpreted = 0;

    bool log_read = false;
    bool log_write = false;

    
public:
    explicit ArmDynarmicCallback(CPUState &parent, DynarmicCPU& cpu)
        : parent(&parent), cpu(&cpu) {}

    ~ArmDynarmicCallback() {}

    uint32_t MemoryReadCode(Dynarmic::A32::VAddr addr) override {
        //LOG_TRACE("Fetch at addr 0x{:X}, inst 0x{} ({})", addr, log_hex(MemoryRead32(addr)), disassemble(*parent, addr, nullptr));
        return MemoryRead32(addr);
    }

    uint8_t MemoryRead8(Dynarmic::A32::VAddr addr) override {
        uint8_t ret = *Ptr<uint8_t>(addr).get(*parent->mem);

        if (log_read) {
            LOG_TRACE("Read uint8_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
        }

        return ret;
    }

    uint16_t MemoryRead16(Dynarmic::A32::VAddr addr) override {
        uint16_t ret = *Ptr<uint16_t>(addr).get(*parent->mem);

        if (log_read) {
            LOG_TRACE("Read uint16_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
        }

        return ret;
    }

    uint32_t MemoryRead32(Dynarmic::A32::VAddr addr) override {
        uint32_t ret = *Ptr<uint32_t>(addr).get(*parent->mem);

        if (log_read) {
            LOG_TRACE("Read uint32_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
        }

        return ret;
    }

    uint64_t MemoryRead64(Dynarmic::A32::VAddr addr) override {
        uint64_t ret = *Ptr<uint64_t>(addr).get(*parent->mem);

        if (log_read) {
            LOG_TRACE("Read uint64_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
        }

        return ret;
    }

    void MemoryWrite8(Dynarmic::A32::VAddr addr, uint8_t value) override {
        *Ptr<uint8_t>(addr).get(*parent->mem) = value;

        if (log_write) {
            LOG_TRACE("Write uint8_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
        }
    }

    void MemoryWrite16(Dynarmic::A32::VAddr addr, uint16_t value) override {
        *Ptr<uint16_t>(addr).get(*parent->mem) = value;

        if (log_write) {
            LOG_TRACE("Write uint16_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
        }
    }

    void MemoryWrite32(Dynarmic::A32::VAddr addr, uint32_t value) override {
        *Ptr<uint32_t>(addr).get(*parent->mem) = value;

        if (log_write) {
            LOG_TRACE("Write uint32_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
        }
    }

    void MemoryWrite64(Dynarmic::A32::VAddr addr, uint64_t value) override {
        *Ptr<uint64_t>(addr).get(*parent->mem) = value;

        if (log_write) {
            LOG_TRACE("Write uint64_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
        }
    }

    void InterpreterFallback(Dynarmic::A32::VAddr addr, size_t num_insts) {
        const bool thumb = cpu->is_thumb_mode();

        if (thumb) {
            addr &= 0xFFFFFFFE;
        } else {
            addr &= 0xFFFFFFFC;
        }
        static std::set<uint32_t> logged;
        static std::mutex mu;
        mu.lock();
        if (logged.find(addr) == logged.end()) {
            LOG_TRACE("Fallback at addr 0x{:X}, inst 0x{:X} ({})", addr, MemoryReadCode(addr), disassemble(*parent, addr, nullptr));
            logged.insert(addr);
        }
        mu.unlock();
        CPUContext context = cpu->save_context();
        cpu->fallback.load_context(context);
        std::uint64_t res = static_cast<std::uint64_t>(cpu->fallback.execute_instructions_no_check(static_cast<int>(num_insts)));
        context = cpu->fallback.save_context();
        cpu->load_context(context);

        interpreted += num_insts;
    }

    void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::Breakpoint:
            return;
        case Dynarmic::A32::Exception::UndefinedInstruction: {
            
            break;
        }
        default:
            LOG_WARN("Exception Raised at pc = 0x{:x}", pc);
            LOG_TRACE("at addr 0x{:X}, inst 0x{:X} ({})", pc, MemoryReadCode(pc), disassemble(*parent, pc, nullptr));

            break;
        }
    }

    void CallSVC(uint32_t svc) override {
        if (svc == 0x44) {
            cpu->stop();
            return;
        }
        parent->call_svc(*parent, svc, cpu->get_pc());
    }

    void AddTicks(uint64_t ticks) override {
        interpreted = 0;
    }

    uint64_t GetTicksRemaining() override {
        return 1ull << 63;
    }
};

std::unique_ptr<Dynarmic::A32::Jit> make_jit(std::unique_ptr<ArmDynarmicCallback> &callback, MemState *mem) {
    Dynarmic::A32::UserConfig config;
    config.callbacks = callback.get();
    config.fastmem_pointer = mem->memory.get();

    if (mem) {
        config.page_table = mem->pages_cpu.get();
    }

    return std::make_unique<Dynarmic::A32::Jit>(config);
}

DynarmicCPU::DynarmicCPU(CPUState *state, Address pc, Address sp, bool log_code)
    : parent(state)
    , fallback(state, pc, sp, log_code)
    , cb(std::make_unique<ArmDynarmicCallback>(*state, *this))
    , ep(pc) {
    jit = make_jit(cb, state->mem);

    set_pc(pc);
    set_lr(pc);
    set_sp(sp);
    set_cpsr((ep & 1) << 5);
    set_fpscr(fallback.get_fpscr());
}

DynarmicCPU::~DynarmicCPU() {
}

/*! Run the CPU */
int DynarmicCPU::run(Address entry_point) {
    if (jit->IsExecuting()) {
        auto ctx = run_worker(*parent, CPUBackend::Dynarmic, entry_point);
        set_reg(0, ctx.cpu_registers[0]);
        return 0;   
    }
    set_lr(parent->halt_instruction_pc | 1);
    jit->Run();
    return 0;
}

int DynarmicCPU::step(Address entry_point) {
    cb->InterpreterFallback(get_pc(), 1);
    return 0;
}

bool DynarmicCPU::is_returning() {
    return false;
}

bool DynarmicCPU::hit_breakpoint() {
    return false;
}

void DynarmicCPU::trigger_breakpoint() {
}

void DynarmicCPU::set_log_code(bool log) {
}

void DynarmicCPU::set_log_mem(bool log) {
}

bool DynarmicCPU::get_log_code() {
    return false;
}

bool DynarmicCPU::get_log_mem() {
    return false;
}

/*! Stop the CPU */
void DynarmicCPU::stop() {
    jit->HaltExecution();
}

/*! Get a specific ARM Rx register. Range from r0 to r15 */
uint32_t DynarmicCPU::get_reg(uint8_t idx) {
    return jit->Regs()[idx];
}

/*! Get the stack pointer */
uint32_t DynarmicCPU::get_sp() {
    return jit->Regs()[13];
}

/*! Get the program counter */
uint32_t DynarmicCPU::get_pc() {
    auto out = jit->Regs()[15];
    return out;
}

/*! Set a Rx register */
void DynarmicCPU::set_reg(uint8_t idx, uint32_t val) {
    jit->Regs()[idx] = val;
}

void DynarmicCPU::set_cpsr(uint32_t val) {
    jit->SetCpsr(val);
}

uint32_t DynarmicCPU::get_tpidruro() {
    return tpidruro;
}

void DynarmicCPU::set_tpidruro(uint32_t val) {
    tpidruro = val;
    fallback.set_tpidruro(val);
}

/*! Set program counter */
void DynarmicCPU::set_pc(uint32_t val) {
    if (val & 1) {
        set_cpsr(get_cpsr() | 0x20);
        val = val & 0xFFFFFFFE;
    } else {
        set_cpsr(get_cpsr() & 0xFFFFFFDF);
        val = val & 0xFFFFFFFC;
    }
    jit->Regs()[15] = val;
}

/*! Set LR */
void DynarmicCPU::set_lr(uint32_t val) {
    jit->Regs()[14] = val;
}

/*! Set stack pointer */
void DynarmicCPU::set_sp(uint32_t val) {
    jit->Regs()[13] = val;
}

uint32_t DynarmicCPU::get_cpsr() {
    return jit->Cpsr();
}

uint32_t DynarmicCPU::get_fpscr() {
    return jit->Fpscr();
}

void DynarmicCPU::set_fpscr(uint32_t val) {
    jit->SetFpscr(val);
}

CPUContext DynarmicCPU::save_context() {
    CPUContext ctx;
    const auto dctx = jit->SaveContext();
    ctx.cpu_registers = dctx.Regs();

    for (uint8_t i = 0; i < ctx.fpu_registers.size(); i++) {
        ctx.fpu_registers[i] = dctx.ExtRegs()[i];
    }

    ctx.fpscr = get_fpscr();
    ctx.cpsr = jit->Cpsr();

    return ctx;
}

void DynarmicCPU::load_context(CPUContext ctx) {
    Dynarmic::A32::Context dctx;
    dctx.Regs() = ctx.cpu_registers;

    for (uint8_t i = 0; i < ctx.fpu_registers.size(); i++) {
        dctx.ExtRegs()[i] = ctx.fpu_registers[i];
    }

    dctx.SetCpsr(ctx.cpsr);
    if (ctx.cpu_registers[15] & 1) {
        dctx.SetCpsr(ctx.cpsr | 0x20);
        dctx.Regs()[15] = ctx.cpu_registers[15] & 0xFFFFFFFE;
    }
    jit->LoadContext(dctx);
    set_fpscr(ctx.fpscr);
}

uint32_t DynarmicCPU::get_lr() {
    return jit->Regs()[14];
}

float DynarmicCPU::get_float_reg(uint8_t idx) {
    return static_cast<float>(jit->ExtRegs()[idx]);
}

void DynarmicCPU::set_float_reg(uint8_t idx, float val) {
    jit->ExtRegs()[idx] = val;
}

bool DynarmicCPU::is_thumb_mode() {
    return jit->Cpsr() & 0x20;
}

