#include <cpu/impl/dynarmic_cpu.h>
#include <cpu/impl/interface.h>
#include <cpu/state.h>
#include <set>
#include <util/log.h>

#include <mem/ptr.h>

class ArmDynarmicCP15 : public Dynarmic::A32::Coprocessor {
    uint32_t tpidruro;

public:
    using CoprocReg = Dynarmic::A32::CoprocReg;

    explicit ArmDynarmicCP15()
        : tpidruro(0) {
    }

    ~ArmDynarmicCP15() {}

    std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, CoprocReg CRd,
        CoprocReg CRn, CoprocReg CRm,
        unsigned opc2) override {
        return std::nullopt;
    }

    CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn,
        CoprocReg CRm, unsigned opc2) override {
        return CallbackOrAccessOneWord{};
    }

    CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) override {
        return CallbackOrAccessTwoWords{};
    }

    CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn, CoprocReg CRm,
        unsigned opc2) override {
        if (CRn == CoprocReg::C13 && CRm == CoprocReg::C0 && opc1 == 0 && opc2 == 3) {
            return &tpidruro;
        }

        return CallbackOrAccessOneWord{};
    }

    CallbackOrAccessTwoWords CompileGetTwoWords(bool two, unsigned opc, CoprocReg CRm) override {
        return CallbackOrAccessTwoWords{};
    }

    std::optional<Callback> CompileLoadWords(bool two, bool long_transfer, CoprocReg CRd,
        std::optional<std::uint8_t> option) override {
        return std::nullopt;
    }

    std::optional<Callback> CompileStoreWords(bool two, bool long_transfer, CoprocReg CRd,
        std::optional<std::uint8_t> option) override {
        return std::nullopt;
    }

    void set_tpidruro(uint32_t tpidruro) {
        this->tpidruro = tpidruro;
    }

    uint32_t get_tpidruro() {
        return tpidruro;
    }
};

class ArmDynarmicCallback : public Dynarmic::A32::UserCallbacks {
    friend class DynarmicCPU;

    CPUState *parent;
    DynarmicCPU *cpu;

public:
    explicit ArmDynarmicCallback(CPUState &parent, DynarmicCPU &cpu)
        : parent(&parent)
        , cpu(&cpu) {}

    ~ArmDynarmicCallback() {}

    uint32_t MemoryReadCode(Dynarmic::A32::VAddr addr) override {
        if (cpu->log_read)
            LOG_TRACE("Fetch at addr 0x{:X}", addr);
        return MemoryRead32(addr);
    }

    template <typename T>
    T MemoryRead(Dynarmic::A32::VAddr addr) {
        Ptr<T> ptr{ addr };
        if (!ptr || !ptr.valid(*parent->mem)) {
            LOG_WARN("Invalid read of uint{}_t at address: 0x{:x}", sizeof(T) * 8, addr);
            return 0;
        }

        T ret = *ptr.get(*parent->mem);
        if (cpu->log_read) {
            LOG_TRACE("Read uint{}_t at address: 0x{:x}, val = 0x{:x}", sizeof(T) * 8, addr, ret);
        }
        return ret;
    }

    uint8_t MemoryRead8(Dynarmic::A32::VAddr addr) override {
        return MemoryRead<uint8_t>(addr);
    }

    uint16_t MemoryRead16(Dynarmic::A32::VAddr addr) override {
        return MemoryRead<uint16_t>(addr);
    }

    uint32_t MemoryRead32(Dynarmic::A32::VAddr addr) override {
        return MemoryRead<uint32_t>(addr);
    }

    uint64_t MemoryRead64(Dynarmic::A32::VAddr addr) override {
        return MemoryRead<uint64_t>(addr);
    }

    template <typename T>
    void MemoryWrite(Dynarmic::A32::VAddr addr, T value) {
        Ptr<T> ptr{ addr };
        if (!ptr || !ptr.valid(*parent->mem)) {
            LOG_TRACE("Invalid write of uint{}_t at addr: 0x{:x}, val = 0x{:x}", sizeof(T) * 8, addr, value);
            return;
        }

        *ptr.get(*parent->mem) = value;
        if (cpu->log_write) {
            LOG_TRACE("Write uint{}_t at addr: 0x{:x}, val = 0x{:x}", sizeof(T) * 8, addr, value);
        }
    }

    void MemoryWrite8(Dynarmic::A32::VAddr addr, uint8_t value) override {
        MemoryWrite<uint8_t>(addr, value);
    }

    void MemoryWrite16(Dynarmic::A32::VAddr addr, uint16_t value) override {
        MemoryWrite<uint16_t>(addr, value);
    }

    void MemoryWrite32(Dynarmic::A32::VAddr addr, uint32_t value) override {
        MemoryWrite<uint32_t>(addr, value);
    }

    void MemoryWrite64(Dynarmic::A32::VAddr addr, uint64_t value) override {
        MemoryWrite<uint64_t>(addr, value);
    }

    bool MemoryWriteExclusive8(Dynarmic::A32::VAddr addr, uint8_t value, uint8_t expected) override {
        return Ptr<uint8_t>(addr).atomic_compare_and_swap(*parent->mem, value, expected);
    }

    bool MemoryWriteExclusive16(Dynarmic::A32::VAddr addr, uint16_t value, uint16_t expected) override {
        return Ptr<uint16_t>(addr).atomic_compare_and_swap(*parent->mem, value, expected);
    }

    bool MemoryWriteExclusive32(Dynarmic::A32::VAddr addr, uint32_t value, uint32_t expected) override {
        return Ptr<uint32_t>(addr).atomic_compare_and_swap(*parent->mem, value, expected);
    }

    bool MemoryWriteExclusive64(Dynarmic::A32::VAddr addr, uint64_t value, uint64_t expected) override {
        return Ptr<uint64_t>(addr).atomic_compare_and_swap(*parent->mem, value, expected);
    }

    void InterpreterFallback(Dynarmic::A32::VAddr addr, size_t num_insts) override {
        if (cpu->is_thumb_mode())
            addr |= 1;

        CPUContext context = cpu->save_context();
        context.set_pc(addr);
        cpu->fallback.load_context(context);
        cpu->fallback.execute_instructions_no_check(static_cast<int>(num_insts));
        context = cpu->fallback.save_context();
        context.cpsr = cpu->get_cpsr();
        context.fpscr = cpu->get_fpscr();
        cpu->load_context(context);
    }

    void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::Breakpoint: {
            cpu->did_break = true;
            cpu->jit->HaltExecution();
            if (cpu->is_thumb_mode())
                cpu->set_pc(pc | 1);
            else
                cpu->set_pc(pc);
            break;
        }
        case Dynarmic::A32::Exception::WaitForInterrupt: {
            cpu->halted = true;
            cpu->jit->HaltExecution();
            break;
        }
        case Dynarmic::A32::Exception::PreloadDataWithIntentToWrite:
        case Dynarmic::A32::Exception::PreloadData:
        case Dynarmic::A32::Exception::PreloadInstruction:
        case Dynarmic::A32::Exception::SendEvent:
        case Dynarmic::A32::Exception::SendEventLocal:
        case Dynarmic::A32::Exception::WaitForEvent:
            break;
        case Dynarmic::A32::Exception::Yield:
            break;
        case Dynarmic::A32::Exception::UndefinedInstruction:
        case Dynarmic::A32::Exception::UnpredictableInstruction:
        case Dynarmic::A32::Exception::DecodeError: {
            LOG_WARN("Undefined/Unpredictable instruction at addr 0x{:X}, inst 0x{:X} ({})", pc, MemoryReadCode(pc), disassemble(*parent, pc, nullptr));
            InterpreterFallback(pc, 1);
            break;
        }
        default:
            LOG_WARN("Unknown exception {} Raised at pc = 0x{:x}", static_cast<size_t>(exception), pc);
            LOG_TRACE("at addr 0x{:X}, inst 0x{:X} ({})", pc, MemoryReadCode(pc), disassemble(*parent, pc, nullptr));
        }
    }

    void CallSVC(uint32_t svc) override {
        parent->protocol->call_svc(*parent, svc, cpu->get_pc(), get_thread_id(*parent));
        if (cpu->exit_request) {
            cpu->jit->HaltExecution();
        }
    }

    void AddTicks(uint64_t ticks) override {}

    uint64_t GetTicksRemaining() override {
        return 1ull << 60;
    }
};

std::unique_ptr<Dynarmic::A32::Jit> DynarmicCPU::make_jit(Dynarmic::ExclusiveMonitor *monitor, bool cpu_opt) {
    Dynarmic::A32::UserConfig config;
    config.arch_version = Dynarmic::A32::ArchVersion::v7;
    config.callbacks = cb.get();
    config.fastmem_pointer = (log_read || log_write) ? nullptr : parent->mem->memory.get();
    config.hook_hint_instructions = true;
    config.global_monitor = monitor;
    config.coprocessors[15] = cp15;
    config.page_table = (log_read || log_write) ? nullptr : parent->mem->pages_cpu.get();
    config.processor_id = core_id;
    config.optimizations = cpu_opt ? Dynarmic::all_safe_optimizations : Dynarmic::no_optimizations;

    return std::make_unique<Dynarmic::A32::Jit>(config);
}

DynarmicCPU::DynarmicCPU(CPUState *state, std::size_t processor_id, Address pc, Address sp, Dynarmic::ExclusiveMonitor *monitor, bool cpu_opt)
    : parent(state)
    , fallback(state, pc, sp)
    , cb(std::make_unique<ArmDynarmicCallback>(*state, *this))
    , cp15(std::make_shared<ArmDynarmicCP15>())
    , ep(pc)
    , core_id(processor_id) {
    jit = make_jit(monitor, cpu_opt);

    set_pc(pc);
    set_lr(pc);
    set_sp(sp);
    set_cpsr((ep & 1) << 5);
    set_fpscr(fallback.get_fpscr());
}

DynarmicCPU::~DynarmicCPU() {
}

int DynarmicCPU::run() {
    halted = false;
    did_break = false;
    exit_request = false;
    jit->Run();
    auto pc = get_pc();
    return halted;
}

int DynarmicCPU::step() {
    jit->Step();
    return 0;
}

bool DynarmicCPU::hit_breakpoint() {
    return did_break;
}

void DynarmicCPU::trigger_breakpoint() {
    did_break = true;
    stop();
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

void DynarmicCPU::stop() {
    exit_request = true;
}

uint32_t DynarmicCPU::get_reg(uint8_t idx) {
    return jit->Regs()[idx];
}

uint32_t DynarmicCPU::get_sp() {
    return jit->Regs()[13];
}

uint32_t DynarmicCPU::get_pc() {
    auto out = jit->Regs()[15];
    return out;
}

void DynarmicCPU::set_reg(uint8_t idx, uint32_t val) {
    jit->Regs()[idx] = val;
}

void DynarmicCPU::set_cpsr(uint32_t val) {
    jit->SetCpsr(val);
}

uint32_t DynarmicCPU::get_tpidruro() {
    return cp15->get_tpidruro();
}

void DynarmicCPU::set_tpidruro(uint32_t val) {
    cp15->set_tpidruro(val);
    fallback.set_tpidruro(val);
}

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

void DynarmicCPU::set_lr(uint32_t val) {
    jit->Regs()[14] = val;
}

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
    ctx.fpu_registers = dctx.ExtRegs();
    ctx.fpscr = jit->Fpscr();
    ctx.cpsr = jit->Cpsr();

    return ctx;
}

void DynarmicCPU::load_context(CPUContext ctx) {
    Dynarmic::A32::Context dctx;
    dctx.Regs() = ctx.cpu_registers;
    dctx.ExtRegs() = ctx.fpu_registers;
    dctx.SetCpsr(ctx.cpsr);
    dctx.SetFpscr(ctx.fpscr);
    if (ctx.cpu_registers[15] & 1) {
        dctx.SetCpsr(ctx.cpsr | 0x20);
        dctx.Regs()[15] = ctx.cpu_registers[15] & 0xFFFFFFFE;
    }
    jit->LoadContext(dctx);
}

uint32_t DynarmicCPU::get_lr() {
    return jit->Regs()[14];
}

float DynarmicCPU::get_float_reg(uint8_t idx) {
    return reinterpret_cast<float &>(jit->ExtRegs()[idx]);
}

void DynarmicCPU::set_float_reg(uint8_t idx, float val) {
    jit->ExtRegs()[idx] = reinterpret_cast<uint32_t &>(val);
}

bool DynarmicCPU::is_thumb_mode() {
    return jit->Cpsr() & 0x20;
}

std::size_t DynarmicCPU::processor_id() const {
    return core_id;
}

void DynarmicCPU::invalidate_jit_cache(Address start, size_t length) {
    jit->InvalidateCacheRange(start, length);
}

// TODO: proper abstraction
ExclusiveMonitorPtr new_exclusive_monitor(int max_num_cores) {
    return new Dynarmic::ExclusiveMonitor(max_num_cores);
}

void free_exclusive_monitor(ExclusiveMonitorPtr monitor) {
    Dynarmic::ExclusiveMonitor *monitor_ = reinterpret_cast<Dynarmic::ExclusiveMonitor *>(monitor);
    delete monitor_;
}

void clear_exclusive(ExclusiveMonitorPtr monitor, std::size_t core_num) {
    Dynarmic::ExclusiveMonitor *monitor_ = reinterpret_cast<Dynarmic::ExclusiveMonitor *>(monitor);
    monitor_->ClearProcessor(core_num);
}
