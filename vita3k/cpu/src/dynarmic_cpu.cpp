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

#include <cpu/common.h>
#include <cpu/disasm/functions.h>
#include <cpu/impl/aliased_thumb.h>
#include <cpu/impl/dynarmic_cpu.h>
#include <cpu/state.h>
#include <util/log.h>

#include <mem/ptr.h>

#include <dynarmic/frontend/A32/a32_ir_emitter.h>
#include <dynarmic/frontend/A32/a32_types.h>
#include <dynarmic/interface/A32/coprocessor.h>
#include <dynarmic/interface/exclusive_monitor.h>

#include <array>
#include <bit>
#include <cstring>
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

static constexpr bool LOG_INVALID_ACCESS = true;

static Dynarmic::ExclusiveMonitor shared_monitor(MAX_CORE_COUNT);

// Why the JIT halts, as the mask Run() and Step() return
static constexpr Dynarmic::HaltReason HALT_STOP = Dynarmic::HaltReason::UserDefined1;
static constexpr Dynarmic::HaltReason HALT_RETURN = Dynarmic::HaltReason::UserDefined2;
static constexpr Dynarmic::HaltReason HALT_BREAKPOINT = Dynarmic::HaltReason::UserDefined3;
static constexpr Dynarmic::HaltReason HALT_SILENT_HILL_WAIT = Dynarmic::HaltReason::UserDefined4;
static constexpr Dynarmic::HaltReason HALT_IMPORT = Dynarmic::HaltReason::UserDefined8;

class ArmDynarmicCallback : public Dynarmic::A32::UserCallbacks {
    DynarmicCPU &cpu;
    uint8_t silent_hill_mismatch_logged = 0;

public:
    explicit ArmDynarmicCallback(DynarmicCPU &cpu)
        : cpu(cpu) {}

    std::optional<std::uint32_t> MemoryReadCode(Dynarmic::A32::VAddr addr) override {
        if (cpu.log_mem)
            LOG_TRACE("Instruction fetch at address 0x{:X}", addr);
        if (addr < KiB(4))
            return std::nullopt;
        if (invalid_access("instruction fetch", addr))
            return std::nullopt;
        return MemoryRead32(addr);
    }

    static void TraceInstruction(uint64_t self_, uint64_t address, uint64_t is_thumb) {
        ArmDynarmicCallback &self = *reinterpret_cast<ArmDynarmicCallback *>(self_);

        const bool valid = is_valid_addr(self.cpu.mem, static_cast<Address>(address));
        const std::string disassembly = valid ? self.cpu.disassemble(address) : "invalid address";
        LOG_TRACE("{} ({}): {} {}", log_hex(self_), self.cpu.thread_id, log_hex(address), disassembly);
    }

    std::optional<uint32_t> read_diagnostic_word(uint32_t address) {
        MemState &mem = cpu.mem;
        if (address < mem.host_page_size || (address & (KiB(4) - 1)) > KiB(4) - sizeof(uint32_t))
            return std::nullopt;

        const std::array ranges{ GuestMemoryRange{ address, sizeof(uint32_t) } };
        const uint64_t pin = pin_guest_memory(mem, ranges);
        if (!pin)
            return std::nullopt;

        std::optional<uint32_t> result;
        {
            const std::lock_guard lock(mem.protect_mutex);
            const auto protected_address = [&](Address addr) {
                const auto it = mem.protect_tree.lower_bound(addr);
                return it != mem.protect_tree.end() && addr < it->first + it->second.size;
            };
            if (!protected_address(address) && !protected_address(address + sizeof(uint32_t) - 1)
                && (!mem.use_page_table || mem.page_table[address / KiB(4)])) {
                uint32_t value;
                std::memcpy(&value, Ptr<const void>(address).get(mem), sizeof(value));
                result = value;
            }
        }
        unpin_guest_memory(mem, pin);
        return result;
    }

    static void StopForSilentHillIntro(uint64_t self_) {
        ArmDynarmicCallback &self = *reinterpret_cast<ArmDynarmicCallback *>(self_);
        self.cpu.jit->HaltExecution(HALT_SILENT_HILL_WAIT);
    }

    void PreCodeTranslationHook(bool is_thumb, Dynarmic::A32::VAddr pc, Dynarmic::A32::IREmitter &ir) override {
        if (cpu.log_code) {
            ir.CallHostFunction(&TraceInstruction, ir.Imm64(reinterpret_cast<uint64_t>(this)), ir.Imm64(pc), ir.Imm64(is_thumb));
        }
    }

    bool PreCodeReadHook(bool is_thumb, Dynarmic::A32::VAddr pc, Dynarmic::A32::IREmitter &ir) override {
        if (cpu.mem.trace_silent_hill && cpu.mem.silent_hill_late_wait_experiment && is_thumb && pc == 0x811AD55E) {
            static constexpr std::array<uint8_t, 18> expected = { 0x40, 0x46, 0x77, 0xf0, 0xcd, 0xfc, 0xda, 0xf8, 0x00, 0x10, 0x50, 0x46, 0xd1, 0xf8, 0xf4, 0x11, 0x88, 0x47 };
            if (!is_valid_addr(cpu.mem, pc) || !is_valid_addr(cpu.mem, pc + expected.size() - 1)
                || std::memcmp(Ptr<const void>(pc).get(cpu.mem), expected.data(), expected.size()) != 0
                || ir.current_location.IT().IsInITBlock()) {
                if ((silent_hill_mismatch_logged & 64) == 0) {
                    silent_hill_mismatch_logged |= 64;
                    LOG_WARN("[SH-EXPERIMENT] Load completion at {:08X} does not match the analyzed executable; wait skipped", pc);
                }
            } else {
                if (ir.block.Location() != static_cast<Dynarmic::IR::LocationDescriptor>(ir.current_location)) {
                    ir.SetTerm(Dynarmic::IR::Term::LinkBlockFast{ ir.current_location });
                    return false;
                }
                // Execute the original MOV R0, R8 before stopping at the following instruction.
                ir.SetRegister(Dynarmic::A32::Reg::R0, ir.GetRegister(Dynarmic::A32::Reg::R8));
                ir.UpdateUpperLocationDescriptor();
                ir.BranchWritePC(ir.Imm32(pc + 2));
                ir.CallHostFunction(&StopForSilentHillIntro, ir.Imm64(reinterpret_cast<uint64_t>(this)));
                ir.SetTerm(Dynarmic::IR::Term::CheckHalt{ Dynarmic::IR::Term::ReturnToDispatch{} });
                return false;
            }
        }
        if (cpu.mem.trace_silent_hill && !cpu.mem.silent_hill_scheduler_experiment && is_thumb && pc == 0x811AD2BA) {
            static constexpr std::array<uint8_t, 14> expected = { 0x44, 0xf2, 0x9c, 0x40, 0xc8, 0xf2, 0x7b, 0x10, 0x01, 0x78, 0x00, 0x29, 0x4d, 0xd1 };
            if (!is_valid_addr(cpu.mem, pc) || !is_valid_addr(cpu.mem, pc + expected.size() - 1)
                || std::memcmp(Ptr<const void>(pc).get(cpu.mem), expected.data(), expected.size()) != 0
                || ir.current_location.IT().IsInITBlock()) {
                if ((silent_hill_mismatch_logged & 8) == 0) {
                    silent_hill_mismatch_logged |= 8;
                    LOG_WARN("[SH-EXPERIMENT] Loading decision at {:08X} does not match the analyzed executable; wait skipped", pc);
                }
            } else {
                if (ir.block.Location() != static_cast<Dynarmic::IR::LocationDescriptor>(ir.current_location)) {
                    ir.SetTerm(Dynarmic::IR::Term::LinkBlockFast{ ir.current_location });
                    return false;
                }
                // Execute the original MOVW before stopping at the following instruction.
                ir.SetRegister(Dynarmic::A32::Reg::R0, ir.Imm32(0x449c));
                ir.UpdateUpperLocationDescriptor();
                ir.BranchWritePC(ir.Imm32(pc + 4));
                ir.CallHostFunction(&StopForSilentHillIntro, ir.Imm64(reinterpret_cast<uint64_t>(this)));
                ir.SetTerm(Dynarmic::IR::Term::CheckHalt{ Dynarmic::IR::Term::ReturnToDispatch{} });
                return false;
            }
        }
        return intercept_aliased_thumb(is_thumb, pc, ir, [this](uint32_t address) { return MemoryReadCode(address); }, [this, pc, &ir]() { PreCodeTranslationHook(true, pc, ir); });
    }

    // Logs and returns true if guest code may not access addr
    bool invalid_access(const char *access, Dynarmic::A32::VAddr addr) {
        MemState &mem = cpu.mem;
        if (addr >= mem.host_page_size && is_valid_addr(mem, addr))
            return false;
        const uint32_t pc = cpu.get_pc();
        if (LOG_INVALID_ACCESS && !mem.invalid_access_logged.test_and_set()) {
            LOG_ERROR("Invalid {} at 0x{:x} by thread {}\n{}", access, addr, cpu.thread_id, cpu.get_context().description());
            if (pc < mem.host_page_size || !is_valid_addr(mem, pc) || pc > UINT32_MAX - 3 || !is_valid_addr(mem, pc + 3)) {
                LOG_CRITICAL("Cannot disassemble invalid PC 0x{:x}", pc);
                const uint32_t lr = cpu.get_lr() & ~1U;
                if (cpu.is_thumb_mode() && lr >= 6 && cpu.get_reg(1) == (pc | 1U)) {
                    const auto first = read_diagnostic_word(lr - 6);
                    const auto second = read_diagnostic_word(lr - 4);
                    std::optional<uint32_t> slot_offset;
                    if (first && second && *first == 0x69c96801 && *second == 0x478869c9) {
                        slot_offset = 0x1c;
                    } else if (lr >= 14 && cpu.get_reg(0) == cpu.get_reg(4)) {
                        const auto load_object = read_diagnostic_word(lr - 14);
                        const auto load_method = read_diagnostic_word(lr - 10);
                        const auto call = read_diagnostic_word(lr - 2);
                        if (load_object && load_method && first && call
                            && *load_object == 0x17d36820 && *load_method == 0x20006b81
                            && *first == 0x1c209000 && (*call & 0xffff) == 0x4788) {
                            slot_offset = 0x38;
                        }
                    }
                    if (slot_offset) {
                        const uint32_t object = cpu.get_reg(0);
                        const auto vtable = read_diagnostic_word(object);
                        if (vtable && *vtable <= UINT32_MAX - *slot_offset) {
                            const auto method = read_diagnostic_word(*vtable + *slot_offset);
                            if (method)
                                LOG_ERROR("Invalid virtual call: object=0x{:08x}, vtable=0x{:08x}, slot=0x{:08x}, target=0x{:08x}", object, *vtable, *vtable + *slot_offset, *method);
                            else
                                LOG_ERROR("Invalid virtual call: object=0x{:08x}, vtable=0x{:08x}, slot unavailable", object, *vtable);
                        } else {
                            LOG_ERROR("Invalid virtual call: object=0x{:08x}, vtable unavailable", object);
                        }
                    }
                }
            } else {
                const auto instruction = read_diagnostic_word(pc);
                if (instruction) {
                    LOG_ERROR("Executing: {}", ::disassemble(cpu.disasm, reinterpret_cast<const uint8_t *>(&*instruction), sizeof(*instruction), pc, cpu.is_thumb_mode()));
                } else {
                    LOG_CRITICAL("Cannot safely disassemble PC 0x{:x}", pc);
                }
            }
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
        LOG_ERROR_SYNC("Unimplemented instruction at address {}", log_hex(addr));
        const auto context = cpu.get_context().description();
        LOG_ERROR_SYNC("Unimplemented instruction at address {}:\n{}", log_hex(addr), context);
        LOG_ERROR("Unimplemented instruction at address {}:\n{}", log_hex(addr), context);
    }

    void log_instruction_exception(uint32_t pc, const char *description) {
        LOG_WARN_SYNC("{} at address 0x{:X}", description, pc);
        const auto instruction = MemoryReadCode(pc).value();
        const auto disassembly = cpu.disassemble(pc, nullptr);
        LOG_WARN_SYNC("{} at address 0x{:X}, instruction 0x{:X} ({})", description, pc, instruction, disassembly);
        LOG_WARN("{} at address 0x{:X}, instruction 0x{:X} ({})", description, pc, instruction, disassembly);
    }

    void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::Breakpoint:
            cpu.jit->HaltExecution(HALT_BREAKPOINT);
            cpu.set_pc(cpu.is_thumb_mode() ? pc | 1 : pc);
            break;
        case Dynarmic::A32::Exception::NoExecuteFault:
            LOG_CRITICAL("PC is 0x{:x}, stopping thread {}\n{}", pc, cpu.thread_id, cpu.get_context().description());
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
            log_instruction_exception(pc, "Undefined instruction");
            InterpreterFallback(pc, 1);
            break;
        case Dynarmic::A32::Exception::UnpredictableInstruction:
            log_instruction_exception(pc, "Unpredictable instruction");
            InterpreterFallback(pc, 1);
            break;
        case Dynarmic::A32::Exception::DecodeError:
            log_instruction_exception(pc, "Decode error");
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

std::unique_ptr<Dynarmic::A32::Jit> DynarmicCPU::make_jit() {
    Dynarmic::A32::UserConfig config{};
    config.arch_version = Dynarmic::A32::ArchVersion::v7;
    config.callbacks = cb.get();
    if (mem.use_page_table) {
        config.page_table = (log_mem || !cpu_opt) ? nullptr : reinterpret_cast<decltype(config.page_table)>(mem.page_table.get());
        config.absolute_offset_page_table = true;
    } else if (!log_mem && cpu_opt) {
        config.fastmem_pointer = std::bit_cast<uintptr_t>(mem.memory.get());
        config.recompile_on_fastmem_failure = true;
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
    if (Dynarmic::Has(reason, HALT_SILENT_HILL_WAIT))
        return { GuestExit::Kind::silent_hill_intro_wait, 0, step };
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

CPUContext DynarmicCPU::get_context() {
    CPUContext ctx;
    ctx.cpu_registers = jit->Regs();
    ctx.fpu_registers = std::bit_cast<std::array<float, 64>>(jit->ExtRegs());
    ctx.fpscr = jit->Fpscr();
    ctx.cpsr = jit->Cpsr();
    ctx.tpidrurw = cp15->tpidrurw;
    return ctx;
}

void DynarmicCPU::set_context(const CPUContext &ctx) {
    jit->Regs() = ctx.cpu_registers;
    jit->ExtRegs() = std::bit_cast<std::array<uint32_t, 64>>(ctx.fpu_registers);
    jit->SetCpsr(ctx.cpsr);
    jit->SetFpscr(ctx.fpscr);
    cp15->tpidrurw = ctx.tpidrurw;
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
