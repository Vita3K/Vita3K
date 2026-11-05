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

#pragma once

#include <cpu/common.h>
#include <cpu/disasm/state.h>
#include <mem/state.h>
#include <util/types.h>

#include <atomic>
#include <initializer_list>
#include <optional>
#include <string>

// The CPU of one guest thread, which a backend like DynarmicCPU implements. It runs the thread's
// guest code when its ThreadState lets it. The executor is the host thread that runs it.
struct CPUState {
    const SceUID thread_id;
    MemState &mem;
    // The core number the kernel gave the thread
    const std::size_t processor_id;
    DisasmState disasm;

    // Return address place_call() writes to LR. Execution stops there.
    Address return_pc = 0;

    // Set by the kubridge fault handler from a signal, read by the CPU after it halts
    std::atomic<bool> abort_pending{ false };
    std::atomic<uint32_t> abort_fault_addr{ 0 };

    virtual ~CPUState() = default;

    enum class RunMode : uint8_t {
        run,
        // One instruction, for the debugger
        single_step,
    };
    // Called by the executor
    // Runs guest code from the PC until it returns, calls an import or stops, or one instruction. A
    // step that a stop request ends before its instruction stops without debugger_stop.
    virtual GuestExit run(RunMode mode) = 0;

    // Called by any thread, also from a signal handler. Takes no lock.
    // Asks the CPU to stop guest code soon
    virtual void request_stop() = 0;

    // Called by any thread, the executor too, but not from a signal handler. It takes the JIT's
    // own lock.
    // Drops the JIT's code for a range of guest memory
    virtual void invalidate_jit_cache(Address start, size_t length) = 0;

    // Called by the executor
    // Clears the exclusive monitor, as the kernel does on its way back to guest code
    virtual void clear_exclusive() = 0;
    // Takes the abort the fault handler noted, if any. The handler runs on any host access to
    // protected guest memory, so the kernel code after an exit can note one too.
    std::optional<Address> take_abort();

    // Called by the executor, or under the ThreadState's mutex for a thread with no run, as its
    // start() does
    // Sets up a call of fn with args: the first four in r0 to r3, the rest on the stack, and the
    // return address in LR
    void place_call(Address fn, std::initializer_list<uint32_t> args);
    // Writes an import's result to the registers the calling convention returns it in
    void write_call_result(const CallResult &result);
    uint32_t stack_alloc(size_t size);
    void stack_free(size_t size);

    // Called by the executor, or by another thread under the ThreadState's mutex while the thread
    // has no run or is parked, or is waiting for a read. A thread that stopped before may run
    // again, so that alone isn't enough.
    // TODO: the gdb stub doesn't take the mutex for them yet.
    virtual uint32_t get_reg(uint8_t idx) = 0;
    virtual void set_reg(uint8_t idx, uint32_t val) = 0;

    virtual uint32_t get_cpsr() = 0;
    virtual void set_cpsr(uint32_t val) = 0;

    virtual uint32_t get_tpidruro() = 0;
    virtual void set_tpidruro(uint32_t val) = 0;

    virtual uint32_t get_tpidrurw() = 0;
    virtual void set_tpidrurw(uint32_t val) = 0;

    virtual float get_float_reg(uint8_t idx) = 0;
    virtual void set_float_reg(uint8_t idx, float val) = 0;

    virtual uint32_t get_fpscr() = 0;
    virtual void set_fpscr(uint32_t val) = 0;

    virtual CPUContext save_context() = 0;
    virtual void load_context(const CPUContext &ctx) = 0;

    uint32_t get_sp() { return get_reg(13); }
    void set_sp(uint32_t val) { set_reg(13, val); }
    uint32_t get_lr() { return get_reg(14); }
    void set_lr(uint32_t val) { set_reg(14, val); }
    uint32_t get_pc() { return get_reg(15); }
    // Sets the PC, and the Thumb state from bit 0
    void set_pc(uint32_t val);
    bool is_thumb_mode() { return get_cpsr() & 0x20; }

    // Debugging helpers
    virtual void set_log_code(bool log) = 0;
    virtual void set_log_mem(bool log) = 0;
    virtual bool get_log_mem() = 0;
    std::string disassemble(uint64_t at, bool thumb, uint16_t *insn_size = nullptr);
    std::string disassemble(uint64_t at, uint16_t *insn_size = nullptr);

protected:
    CPUState(MemState &mem, SceUID thread_id, std::size_t processor_id);
};

CPUStatePtr init_cpu(bool cpu_opt, SceUID thread_id, std::size_t processor_id, MemState &mem);

// The CPU state running on this host thread, for signal handlers
void set_current_cpu_state(CPUState *state);
CPUState *get_current_cpu_state();
