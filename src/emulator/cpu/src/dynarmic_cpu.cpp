#include <cpu/dynarmic_cpu.h>
#include <cpu/interface.h>

#include <util/log.h>

#include <mem/ptr.h>

class ArmDynarmicCallback: public Dynarmic::A32::UserCallbacks {
    friend class DynarmicCPU;
    
    CPUState *parent;

    uint64_t interpreted = 0;

    bool log_read = false;
    bool log_write = false;
    
public:
    explicit ArmDynarmicCallback(CPUState &parent)
        : parent(&parent) {}

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
        DynarmicCPU &dyncpu = *std::dynamic_pointer_cast<DynarmicCPU>(parent->cpu);

        CPUInterface::ThreadContext context = dyncpu.save_context();
        dyncpu.fallback.load_context(context);
        dyncpu.fallback.execute_instructions(num_insts);
        context = dyncpu.fallback.save_context();
        dyncpu.load_context(context);

        interpreted += num_insts;
    }

    void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::Breakpoint:
        case Dynarmic::A32::Exception::UndefinedInstruction:
            return;
        default:
            LOG_WARN("Exception Raised at pc = 0x{:x}", pc);
            break;
        }
    }

    void CallSVC(uint32_t svc) override {
        parent->call_svc(*parent, svc, parent->cpu->get_pc());
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

    if (mem) {
        std::array<uint8_t*, Dynarmic::A32::UserConfig::NUM_PAGE_TABLE_ENTRIES> pages;

        pages[0] = mem->memory.get();

        for (size_t i = 0; i < mem->allocated_pages.size(); i++) {
            pages[i] = pages[i-1] + mem->page_size;
        }

        config.page_table = &pages;
    }

    return std::make_unique<Dynarmic::A32::Jit>(config);
}

DynarmicCPU::DynarmicCPU(CPUState *state, Address pc, Address sp, bool log_code)
    : fallback(state, pc, sp, log_code) 
    , cb(std::make_unique<ArmDynarmicCallback>(*this)) {
    jit = make_jit(cb, state->mem);
}

/*! Run the CPU */
void DynarmicCPU::run() {
    jit->Run();
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
    return jit->Regs()[15];
}

/*! Set a Rx register */
void DynarmicCPU::set_reg(uint8_t idx, uint32_t val) {
    jit->Regs()[idx] = val;
}

void DynarmicCPU::set_cpsr(uint32_t val) {
    jit->SetCpsr(val);
}

/*! Set program counter */
void DynarmicCPU::set_pc(uint32_t val) {
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

uint32_t DynarmicCPU::get_lr() {
    return jit->Regs[14];
}

float DynarmicCPU::get_float_reg(uint8_t idx) {
    return 0;
}

DynarmicCPU::ThreadContext DynarmicCPU::save_context() {
    ThreadContext ctx;

    ctx.cpsr = get_cpsr();

    for (uint8_t i = 0; i < 16; i++) {
        ctx.cpu_registers[i] = get_reg(i);
    }

    ctx.pc = get_pc();
    ctx.sp = get_sp();

    return ctx;
}

void DynarmicCPU::load_context(UnicornCPU::ThreadContext ctx) {
    jit->SetCpsr(ctx.cpsr);

    for (uint8_t i = 0; i < 16; i++) {
        jit->Regs()[i] = ctx.cpu_registers[i];
    }

    set_pc(ctx.pc);
    set_sp(ctx.sp);
}