// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "cpu/common.h"
#include <cpu/impl/dynarmic_cpu.h>
#include <cpu/state.h>
#include <util/log.h>

#include <mem/ptr.h>

#include <dynarmic/frontend/A32/a32_ir_emitter.h>
#include <dynarmic/interface/A32/coprocessor.h>
#include <dynarmic/interface/exclusive_monitor.h>

#include <bit>
#include <memory>
#include <optional>
#include <string>

class ArmDynarmicCP15 : public Dynarmic::A32::Coprocessor {
public:
    using CoprocReg = Dynarmic::A32::CoprocReg;

    uint32_t tpidruro = 0;
    uint32_t tpidrurw = 0;
    uint32_t sctlr = 0;
    uint32_t dacr = 0;

    std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, CoprocReg CRd,
        CoprocReg CRn, CoprocReg CRm,
        unsigned opc2) override {
        return std::nullopt;
    }

    // MCR p15, 0, Rt, CRn, c0, opc2 for TPIDRURO (c13, 3), TPIDRURW (c13, 2), SCTLR (c1, 0) and DACR (c3, 0)
    CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, CoprocReg CRn,
        CoprocReg CRm, unsigned opc2) override {
        if (opc1 == 0 && CRm == CoprocReg::C0) {
            if (CRn == CoprocReg::C13 && opc2 == 3)
                return &tpidruro;
            if (!two && CRn == CoprocReg::C13 && opc2 == 2)
                return &tpidrurw;
            if (!two && CRn == CoprocReg::C1 && opc2 == 0)
                return &sctlr;
            if (!two && CRn == CoprocReg::C3 && opc2 == 0)
                return &dacr;
        }
        LOG_WARN("Unhandled CP15 MCR or MRC: two={} opc1={} CRn={} CRm={} opc2={}", two, opc1, static_cast<int>(CRn), static_cast<int>(CRm), opc2);
        return CallbackOrAccessOneWord{};
    }

    CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, CoprocReg CRm) override {
        return CallbackOrAccessTwoWords{};
    }

    // MRC reads the same registers
    CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, CoprocReg CRn, CoprocReg CRm,
        unsigned opc2) override {
        return CompileSendOneWord(two, opc1, CRn, CRm, opc2);
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
};

static constexpr bool LOG_INVALID_ACCESS = false;

// Why the JIT halts, as the mask Run() and Step() return
static constexpr Dynarmic::HaltReason HALT_STOP = Dynarmic::HaltReason::UserDefined1;
static constexpr Dynarmic::HaltReason HALT_RETURN = Dynarmic::HaltReason::UserDefined2;
static constexpr Dynarmic::HaltReason HALT_BREAKPOINT = Dynarmic::HaltReason::UserDefined3;
static constexpr Dynarmic::HaltReason HALT_IMPORT = Dynarmic::HaltReason::UserDefined8;

class ArmDynarmicCallback : public Dynarmic::A32::UserCallbacks {
    DynarmicCPU &cpu;

public:
    explicit ArmDynarmicCallback(DynarmicCPU &cpu)
        : cpu(cpu) {}

    std::optional<std::uint32_t> MemoryReadCode(Dynarmic::A32::VAddr addr) override {
        if (cpu.log_mem)
            LOG_TRACE("Instruction fetch at address 0x{:X}", addr);
        if (addr < KiB(4))
            return std::nullopt;
        return MemoryRead32(addr);
    }

    static void TraceInstruction(uint64_t self_, uint64_t address, uint64_t is_thumb) {
        ArmDynarmicCallback &self = *reinterpret_cast<ArmDynarmicCallback *>(self_);

        const bool valid = is_valid_addr(self.cpu.mem, static_cast<Address>(address));
        const std::string disassembly = valid ? self.cpu.disassemble(address) : "invalid address";
        LOG_TRACE("{} ({}): {} {}", log_hex(self_), self.cpu.thread_id, log_hex(address), disassembly);
    }

    void PreCodeTranslationHook(bool is_thumb, Dynarmic::A32::VAddr pc, Dynarmic::A32::IREmitter &ir) override {
        if (cpu.log_code) {
            ir.CallHostFunction(&TraceInstruction, ir.Imm64(reinterpret_cast<uint64_t>(this)), ir.Imm64(pc), ir.Imm64(is_thumb));
        }
    }

    // Logs and returns true if guest code may not access addr
    bool invalid_access(const char *access, Dynarmic::A32::VAddr addr) {
        MemState &mem = cpu.mem;
        if (addr >= mem.host_page_size && is_valid_addr(mem, addr))
            return false;
        const uint32_t pc = cpu.get_pc();
        if (LOG_INVALID_ACCESS) {
            LOG_ERROR("Invalid {} at 0x{:x}\n{}", access, addr, cpu.save_context().description());
            if (pc < mem.host_page_size)
                LOG_CRITICAL("PC is 0x{:x}", pc);
            else
                LOG_ERROR("Executing: {}", cpu.disassemble(pc, nullptr));
        } else if (!mem.invalid_access_logged.test_and_set()) {
            LOG_ERROR("Invalid {} at 0x{:x} by thread {} (pc 0x{:x}). Set LOG_INVALID_ACCESS for full reports", access, addr, cpu.thread_id, pc);
        } else {
            LOG_ERROR("Invalid {} at 0x{:x} by thread {} (pc 0x{:x})", access, addr, cpu.thread_id, pc);
        }
        return true;
    }

    template <typename T>
    T MemoryRead(Dynarmic::A32::VAddr addr) {
        if (invalid_access("read", addr))
            return 0;
        const T ret = *Ptr<T>(addr).get(cpu.mem);
        if (cpu.log_mem) {
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
        if (invalid_access("write", addr))
            return;
        *Ptr<T>(addr).get(cpu.mem) = value;
        if (cpu.log_mem) {
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

    template <typename T>
    bool MemoryWriteExclusive(Dynarmic::A32::VAddr addr, T value, T expected) {
        if (invalid_access("exclusive write", addr))
            return false;
        const bool result = Ptr<T>(addr).atomic_compare_and_swap(cpu.mem, value, expected);
        if (cpu.log_mem) {
            LOG_TRACE("Write uint{}_t at addr: 0x{:x}, val = 0x{:x}, expected = 0x{:x}", sizeof(T) * 8, addr, value, expected);
        }
        return result;
    }

    bool MemoryWriteExclusive8(Dynarmic::A32::VAddr addr, uint8_t value, uint8_t expected) override {
        return MemoryWriteExclusive(addr, value, expected);
    }

    bool MemoryWriteExclusive16(Dynarmic::A32::VAddr addr, uint16_t value, uint16_t expected) override {
        return MemoryWriteExclusive(addr, value, expected);
    }

    bool MemoryWriteExclusive32(Dynarmic::A32::VAddr addr, uint32_t value, uint32_t expected) override {
        return MemoryWriteExclusive(addr, value, expected);
    }

    bool MemoryWriteExclusive64(Dynarmic::A32::VAddr addr, uint64_t value, uint64_t expected) override {
        return MemoryWriteExclusive(addr, value, expected);
    }

    void InterpreterFallback(Dynarmic::A32::VAddr addr, size_t num_insts) override {
        LOG_ERROR("Unimplemented instruction at address {}:\n{}", log_hex(addr), cpu.save_context().description());
    }

    void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::Breakpoint:
            cpu.jit->HaltExecution(HALT_BREAKPOINT);
            cpu.set_pc(cpu.is_thumb_mode() ? pc | 1 : pc);
            break;
        case Dynarmic::A32::Exception::NoExecuteFault:
            LOG_CRITICAL("PC is 0x{:x}, stopping thread {}\n{}", pc, cpu.thread_id, cpu.save_context().description());
            [[fallthrough]];
        case Dynarmic::A32::Exception::WaitForInterrupt:
            cpu.jit->HaltExecution(HALT_RETURN);
            break;
        case Dynarmic::A32::Exception::PreloadDataWithIntentToWrite:
        case Dynarmic::A32::Exception::PreloadData:
        case Dynarmic::A32::Exception::PreloadInstruction:
        case Dynarmic::A32::Exception::SendEvent:
        case Dynarmic::A32::Exception::SendEventLocal:
        case Dynarmic::A32::Exception::WaitForEvent:
        case Dynarmic::A32::Exception::Yield:
            break;
        case Dynarmic::A32::Exception::UndefinedInstruction:
            LOG_WARN("Undefined instruction at address 0x{:X}, instruction 0x{:X} ({})", pc, MemoryReadCode(pc).value(), cpu.disassemble(pc, nullptr));
            InterpreterFallback(pc, 1);
            break;
        case Dynarmic::A32::Exception::UnpredictableInstruction:
            LOG_WARN("Unpredictable instruction at address 0x{:X}, instruction 0x{:X} ({})", pc, MemoryReadCode(pc).value(), cpu.disassemble(pc, nullptr));
            InterpreterFallback(pc, 1);
            break;
        case Dynarmic::A32::Exception::DecodeError:
            LOG_WARN("Decode error at address 0x{:X}, instruction 0x{:X} ({})", pc, MemoryReadCode(pc).value(), cpu.disassemble(pc, nullptr));
            InterpreterFallback(pc, 1);
            break;
        default:
            LOG_WARN("Unknown exception {} Raised at pc = 0x{:x}", static_cast<size_t>(exception), pc);
            LOG_TRACE("at address 0x{:X}, instruction 0x{:X} ({})", pc, MemoryReadCode(pc).value(), cpu.disassemble(pc, nullptr));
        }
    }

    void CallSVC(uint32_t) override {
        cpu.jit->HaltExecution(HALT_IMPORT);
    }

    void AddTicks(uint64_t ticks) override {}

    uint64_t GetTicksRemaining() override {
        return 1ull << 60;
    }
};

static Dynarmic::ExclusiveMonitor shared_monitor(MAX_CORE_COUNT);

std::unique_ptr<Dynarmic::A32::Jit> DynarmicCPU::make_jit() {
    Dynarmic::A32::UserConfig config{};
    config.arch_version = Dynarmic::A32::ArchVersion::v7;
    config.callbacks = cb.get();
    if (mem.use_page_table) {
        config.page_table = (log_mem || !cpu_opt) ? nullptr : reinterpret_cast<decltype(config.page_table)>(mem.page_table.get());
        config.absolute_offset_page_table = true;
    } else if (!log_mem && cpu_opt) {
        config.fastmem_pointer = std::bit_cast<uintptr_t>(mem.memory.get());
    }
    config.hook_hint_instructions = true;
    config.global_monitor = &shared_monitor;
    config.coprocessors[15] = cp15;
    config.processor_id = processor_id;
    config.optimizations = cpu_opt ? Dynarmic::all_safe_optimizations : Dynarmic::no_optimizations;
    config.enable_cycle_counting = false;

    return std::make_unique<Dynarmic::A32::Jit>(config);
}

DynarmicCPU::DynarmicCPU(MemState &mem, SceUID thread_id, std::size_t processor_id, bool cpu_opt)
    : CPUState(mem, thread_id, processor_id)
    , cb(std::make_unique<ArmDynarmicCallback>(*this))
    , cp15(std::make_shared<ArmDynarmicCP15>())
    , cpu_opt(cpu_opt) {
    jit = make_jit();
}

DynarmicCPU::~DynarmicCPU() = default;

GuestExit DynarmicCPU::run(RunMode mode) {
    const bool step = mode == RunMode::single_step;
    Dynarmic::HaltReason reason;
    if (step) {
        reason = jit->Step();
    } else {
        // Run until the JIT stops for something other than a step or a cache invalidation
        do {
            reason = jit->Run();
        } while (reason == Dynarmic::HaltReason::Step || reason == Dynarmic::HaltReason::CacheInvalidation);
    }
    // The NID is the word after the SVC. A stepped SVC reports the debugger stop with it.
    if (Dynarmic::Has(reason, HALT_IMPORT))
        return { GuestExit::Kind::import, *Ptr<uint32_t>(get_pc() + 4).get(mem), step };
    // A run that halted stopped at the return address, and a step never returns
    if (!step && Dynarmic::Has(reason, HALT_RETURN))
        return { GuestExit::Kind::returned, get_reg(0) };
    // A breakpoint, or a step that ran its instruction. A step that a pending stop request ended
    // before its instruction has neither.
    return { GuestExit::Kind::stopped, 0, Dynarmic::Has(reason, Dynarmic::HaltReason::Step | HALT_BREAKPOINT) };
}

void DynarmicCPU::clear_exclusive() {
    shared_monitor.ClearProcessor(processor_id);
}

void DynarmicCPU::set_log_code(bool log) {
    if (log_code == log)
        return;

    log_code = log;
    jit = make_jit();
}

void DynarmicCPU::set_log_mem(bool log) {
    if (log_mem == log)
        return;

    log_mem = log;
    jit = make_jit();
}

bool DynarmicCPU::get_log_mem() {
    return log_mem;
}

void DynarmicCPU::request_stop() {
    jit->HaltExecution(HALT_STOP);
}

uint32_t DynarmicCPU::get_reg(uint8_t idx) {
    return jit->Regs()[idx];
}

void DynarmicCPU::set_reg(uint8_t idx, uint32_t val) {
    jit->Regs()[idx] = val;
}

void DynarmicCPU::set_cpsr(uint32_t val) {
    jit->SetCpsr(val);
}

uint32_t DynarmicCPU::get_tpidruro() {
    return cp15->tpidruro;
}

void DynarmicCPU::set_tpidruro(uint32_t val) {
    cp15->tpidruro = val;
}

uint32_t DynarmicCPU::get_tpidrurw() {
    return cp15->tpidrurw;
}

void DynarmicCPU::set_tpidrurw(uint32_t val) {
    cp15->tpidrurw = val;
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
    ctx.cpu_registers = jit->Regs();
    ctx.fpu_registers = std::bit_cast<std::array<float, 64>>(jit->ExtRegs());
    ctx.fpscr = jit->Fpscr();
    ctx.cpsr = jit->Cpsr();
    return ctx;
}

void DynarmicCPU::load_context(const CPUContext &ctx) {
    jit->Regs() = ctx.cpu_registers;
    jit->ExtRegs() = std::bit_cast<std::array<uint32_t, 64>>(ctx.fpu_registers);
    jit->SetCpsr(ctx.cpsr);
    jit->SetFpscr(ctx.fpscr);
}

float DynarmicCPU::get_float_reg(uint8_t idx) {
    return std::bit_cast<float>(jit->ExtRegs()[idx]);
}

void DynarmicCPU::set_float_reg(uint8_t idx, float val) {
    jit->ExtRegs()[idx] = std::bit_cast<uint32_t>(val);
}

void DynarmicCPU::invalidate_jit_cache(Address start, size_t length) {
    jit->InvalidateCacheRange(start, length);
}
