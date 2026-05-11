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

#include <cpu/functions.h>
#include <kernel/thread/thread_state.h>

#include <kernel/state.h>
#include <mem/ptr.h>
#include <util/align.h>

#include <util/log.h>

#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>

void ThreadSignal::wait() {
    std::unique_lock<std::mutex> lock(mutex);
    recv_cond.wait(lock, [&]() { return signaled; });
    signaled = false;
}

bool ThreadSignal::send() {
    std::unique_lock<std::mutex> lock(mutex);
    if (signaled) {
        return false;
    }
    signaled = true;
    recv_cond.notify_one();
    return true;
}

int ThreadState::init(const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, const SceKernelThreadOptParam *option = nullptr) {
    constexpr size_t KERNEL_TLS_SIZE = 0x800;

    // the stack size should be page-aligned
    stack_size = align(stack_size, KiB(4));

    this->name = name;
    this->entry_point = entry_point.address();

    int core_num = kernel.corenum_allocator.new_corenum();
    if (core_num < 0) {
        LOG_ERROR("Out of core number to allocate, use 0");
        core_num = 0;
    }

    if (init_priority > SCE_KERNEL_LOWEST_PRIORITY_USER) {
        assert(SCE_KERNEL_HIGHEST_DEFAULT_PRIORITY <= init_priority && init_priority <= SCE_KERNEL_LOWEST_DEFAULT_PRIORITY);
        priority = init_priority - SCE_KERNEL_DEFAULT_PRIORITY + SCE_KERNEL_GAME_DEFAULT_PRIORITY_ACTUAL;
    } else {
        priority = init_priority;
    }
    this->affinity_mask = affinity_mask;
    this->stack_size = stack_size;
    start_tick = rtc_get_ticks(kernel.base_tick.tick);
    last_vblank_waited = 0;

    cpu = init_cpu(kernel.cpu_opt, id, static_cast<std::size_t>(core_num), mem);
    if (!cpu) {
        return SCE_KERNEL_ERROR_ERROR;
    }
    if (kernel.debugger.watch_code) {
        set_log_code(*cpu, true);
    }
    if (kernel.debugger.watch_memory) {
        set_log_mem(*cpu, true);
    }

    std::string alloc_name = fmt::format("Stack for thread {} (#{})", name, id);
    stack = alloc_block(mem, stack_size, alloc_name.c_str());
    memset(stack.get_ptr<void>().get(mem), 0xcc, stack_size);

    alloc_name = fmt::format("TLS for thread {} (#{})", name, id);
    const size_t tls_size = KERNEL_TLS_SIZE + kernel.tls_msize;
    tls = alloc_block(mem, tls_size, alloc_name.c_str());
    const Ptr<uint8_t> base_tls_ptr = tls.get_ptr<uint8_t>();
    memset(base_tls_ptr.get(mem), 0, tls_size);

    int *tls_array = tls.get_ptr<int>().get(mem);

    tls_array[TLS_PROCESS_ID] = 1; // stubbed. unused
    tls_array[TLS_THREAD_ID] = id;
    tls_array[TLS_SP_TOP] = stack.get();
    tls_array[TLS_SP_BOTTOM] = stack.get() + stack_size;
    tls_array[TLS_CURRENT_PRIORITY] = priority;
    tls_array[TLS_CPU_AFFINITY_MASK] = affinity_mask;

    const Ptr<uint8_t> user_tls_ptr = base_tls_ptr + KERNEL_TLS_SIZE;
    write_tpidruro(*cpu, user_tls_ptr.address());
    if (kernel.tls_address) {
        assert(kernel.tls_psize <= kernel.tls_msize);
        memcpy(user_tls_ptr.get(mem), kernel.tls_address.get(mem), kernel.tls_psize);
    }

    CPUContext ctx;
    ctx.set_sp(stack_top());
    if (option) {
        ctx.cpu_registers[0] = option->attr;
        ctx.cpu_registers[1] = option->size;
    }
    this->init_cpu_ctx = ctx;

    return 0;
}

void ThreadState::raise_waiting_threads() {
    for (const auto &t : waiting_threads) {
        const std::unique_lock<std::mutex> lock(t->mutex);
        assert(t->status == ThreadStatus::wait);
        t->status = ThreadStatus::run;
        t->status_cond.notify_all();
    }
    waiting_threads.clear();
}

int ThreadState::start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback) {
    std::unique_lock<std::mutex> thread_lock(mutex);
    if (status != ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_RUNNING;

    run_start_callback = run_entry_callback;
    load_context(*cpu, init_cpu_ctx);
    write_pc(*cpu, entry_point);
    write_lr(*cpu, kernel.halt_instruction_pc);
    write_reg(*cpu, 0, arglen);

    // Copy data to stack
    if (argp && arglen > 0) {
        const Address data_addr = stack_alloc(*cpu, align(arglen, 8));
        memcpy(Ptr<uint8_t>(data_addr).get(mem), argp.get(mem), arglen);
        write_reg(*cpu, 1, data_addr);
    } else {
        write_reg(*cpu, 1, 0);
    }

    if (kernel.debugger.wait_for_debugger) {
        kernel.debugger.wait_for_debugger = false;
        status = ThreadStatus::suspend;
    } else {
        status = ThreadStatus::run;
    }
    status_cond.notify_one();

    return SCE_KERNEL_OK;
}

void ThreadState::exit(SceInt32 status) {
    std::lock_guard<std::mutex> guard(mutex);
    run_end_callback = true;
    exit_requested = true;
    returned_value = static_cast<uint32_t>(status);
}

void ThreadState::exit_delete(bool exit) {
    std::lock_guard<std::mutex> lock(mutex);

    run_end_callback = exit;
    delete_requested = true;

    if (status == ThreadStatus::run) {
        stop(*cpu);
    } else if (status == ThreadStatus::wait) {
        // wake threads blocked in a sync primitive so they can observe delete_requested
        update_status(ThreadStatus::run);
    } else {
        // dormant or suspend: wake run_loop() so it can observe delete_requested
        status_cond.notify_all();
    }

    // Wake if thread waiting on sceKernelWaitSignal
    signal.send();
}

void ThreadState::run_loop() {
    bool guest_returned = false;

    // Set thread-local CPU state so signal handlers can access it.
    // The guard clears it on any exit so a recycled host thread never sees
    // a stale CPUState pointer.
    set_current_cpu_state(cpu.get());
    struct CpuStateGuard {
        ~CpuStateGuard() { set_current_cpu_state(nullptr); }
    } cpu_state_guard;

    std::unique_lock<std::mutex> lock(mutex);
    ++call_level;
    const bool top_level = call_level == 1;

    auto run_thread_end_callback = [&]() {
        if (!run_end_callback)
            return;
        run_end_callback = false;

        if (!kernel.thread_event_end)
            return;

        const ThreadStatus old_status = status;
        const uint32_t old_returned_value = returned_value;
        status = ThreadStatus::run;

        lock.unlock();
        const int ret = run_callback(kernel.thread_event_end.address(), { SCE_KERNEL_THREAD_EVENT_TYPE_END, static_cast<uint32_t>(id), 0, kernel.thread_event_end_arg });
        if (ret != 0)
            LOG_WARN("Thread end event handler returned {}", log_hex(ret));
        lock.lock();

        status = old_status;
        returned_value = old_returned_value;
    };

    while (true) {
        // Check if this call-level is done (normal exit, guest return, or delete).
        if (exit_requested || guest_returned || delete_requested) {
            // Top-level fires the end callback and parks dormant
            if (top_level && status != ThreadStatus::dormant) {
                run_thread_end_callback();
                update_status(ThreadStatus::dormant);
            }
            // Return from nested levels to the top level (or completely if deleted)
            if (!top_level || delete_requested)
                break;
            exit_requested = false;
            guest_returned = false;
            // Status is now dormant: we'll park until the next start() or exit_delete().
        }

        // Park until we have something to do.
        if (status != ThreadStatus::run) {
            status_cond.wait(lock, [&] {
                return status == ThreadStatus::run || delete_requested;
            });
            continue;
        }

        // Fire the thread-start event once per start.
        if (run_start_callback) {
            run_start_callback = false;
            lock.unlock();
            if (kernel.thread_event_start) {
                const int ret = run_callback(kernel.thread_event_start.address(), { SCE_KERNEL_THREAD_EVENT_TYPE_START, static_cast<uint32_t>(id), 0, kernel.thread_event_start_arg });
                if (ret != 0)
                    LOG_WARN("Thread start event handler returned {}", log_hex(ret));
            }
            lock.lock();
        }

        // Active JIT loop. Lock held on entry and exit; unlocked only around run/step.
        while (!delete_requested && !exit_requested && !guest_returned && status == ThreadStatus::run) {
            const bool do_step = single_stepping;
            if (do_step)
                single_stepping = false;

            lock.unlock();

            // Single step or run
            const int res = do_step ? step(*cpu) : run(*cpu);

            // handle svc call if this was what stopped the cpu
            if (cpu->svc_called) {
                const uint32_t nid = *Ptr<uint32_t>(read_pc(*cpu) + 4).get(mem);
                kernel.call_import(*cpu, nid, id);
                clear_exclusive(*cpu);
            }

            // handle pending abort (exception handler from page fault)
            if (cpu->abort_pending.exchange(false))
                dispatch_abort(*cpu);

            lock.lock();

            if (do_step || suspend_requested || hit_breakpoint(*cpu)) {
                suspend_requested = false;
                update_status(ThreadStatus::suspend);
            }

            // Guest function for this run_loop returned (or errored).
            if (res != 0) {
                if (res < 0) {
                    LOG_ERROR("Thread {} ({}) experienced a cpu error.", name, cpu->thread_id);
                    returned_value = 0xDEADDEAD;
                } else {
                    // Halt-sentinel (res = 1): guest function returned cleanly.
                    returned_value = read_reg(*cpu, 0);
                }
                guest_returned = true;
            }
        }
    }

    --call_level;
}

void ThreadState::push_arguments(const std::vector<uint32_t> &args) {
    Address sp = read_sp(*cpu);
    for (size_t i = 0; i < std::min(args.size(), static_cast<size_t>(4)); i++) {
        write_reg(*cpu, i, args[i]);
    }
    if (args.size() > 4) {
        // TODO align to 16 bytes
        const size_t remain_size = args.size() - 4;
        sp -= 4 * remain_size;
        memcpy(Ptr<uint32_t>(sp).get(mem), &args[4], remain_size * 4);
    }
    write_sp(*cpu, sp);
}

uint32_t ThreadState::run_callback(Address callback_address, const std::vector<uint32_t> &args) {
    std::unique_lock<std::mutex> thread_lock(mutex);
    if (call_level == 0) {
        LOG_ERROR("run_callback should not be called as the first thread entry");
        return 0;
    }

    // save the current context before overwriting PC/LR for the callback
    const CPUContext previous_ctx = save_context(*cpu);
    const uint32_t previous_tpidruro = read_tpidruro(*cpu);

    // we shouldn't have to clean the context I believe
    write_pc(*cpu, callback_address);
    write_lr(*cpu, kernel.halt_instruction_pc);
    push_arguments(args);
    thread_lock.unlock();

    // unlock but then immediately lock back in the run_loop function
    // shouldn't cause an issue, but maybe we could use a recursive mutex instead
    run_loop();

    thread_lock.lock();

    // restore the previous context
    // actually, in most case I don't think this is necessary as the caller
    // and the callee should respect the same calling convention
    // but do it just in case
    load_context(*cpu, previous_ctx);
    write_tpidruro(*cpu, previous_tpidruro);

    return returned_value;
}

void ThreadState::dispatch_abort(CPUState &cpu) {
    const uint32_t fault_addr = cpu.abort_fault_addr.load();
    // DABT = type 0
    const Address handler = kernel.exception_handlers[0].load();
    if (!handler)
        return;

    // Build KuKernelAbortContext on guest stack for the handler to read.
    // Note: by the time we get here, the page has already been unprotected
    // by the protect_tree mechanism, and the CPU may have executed past
    // the faulting instruction. The handler is called as a notification;
    // we don't restore from AbortContext afterward.
    // { r0-r12, sp, lr, pc, FAR } = 17 uint32_t = 68 bytes
    const uint32_t ctx_size = 17 * 4;
    const uint32_t sp_orig = read_sp(cpu);
    const uint32_t sp_aligned = align_down(sp_orig - ctx_size, 8);

    auto *ctx = Ptr<uint32_t>(sp_aligned).get(*cpu.mem);
    for (int i = 0; i < 13; i++)
        ctx[i] = read_reg(cpu, i);
    ctx[13] = sp_aligned + ctx_size;
    ctx[14] = read_lr(cpu);
    ctx[15] = read_pc(cpu);
    ctx[16] = fault_addr;

    LOG_DEBUG("DABT handler=0x{:08X} FAR=0x{:08X} PC=0x{:08X} SP=0x{:08X} sp_aligned=0x{:08X}",
        handler, fault_addr, ctx[15], sp_orig, sp_aligned);

    // run_callback saves/restores full CPU context internally
    run_callback(handler, { sp_aligned });
}

uint32_t ThreadState::run_guest_function(Address callback_address, SceSize args, const Ptr<void> argp) {
    // save the previous entry point, just in case
    const auto old_entry_point = entry_point;
    entry_point = callback_address;

    start(args, argp);
    {
        std::unique_lock<std::mutex> lock(mutex);
        status_cond.wait(lock, [&]() { return status == ThreadStatus::dormant; });
    }

    entry_point = old_entry_point;
    return returned_value;
}

ThreadState::ThreadState(SceUID id, KernelState &kernel, MemState &mem)
    : id(id)
    , kernel(kernel)
    , mem(mem) {
}

void ThreadState::update_status(ThreadStatus status, std::optional<ThreadStatus> expected) {
    if (expected)
        assert(expected.value() == this->status);

    // Don't apply the requested wait transition if being removed to not block deletion
    if (status == ThreadStatus::wait && delete_requested)
        return;

    this->status = status;
    status_cond.notify_all();

    if (status == ThreadStatus::dormant) {
        raise_waiting_threads();
    }
}

Address ThreadState::stack_top() const {
    return stack.get() + stack_size;
}

void ThreadState::suspend() {
    assert(status == ThreadStatus::run);
    {
        const std::lock_guard<std::mutex> lock(mutex);
        suspend_requested = true;
    }
    stop(*cpu);
}

void ThreadState::resume(bool step) {
    assert(status == ThreadStatus::suspend || status == ThreadStatus::dormant);
    {
        const std::lock_guard<std::mutex> lock(mutex);
        single_stepping = step;
        update_status(ThreadStatus::run);
    }
}

std::string ThreadState::log_stack_traceback() const {
    constexpr Address START_OFFSET = 0;
    constexpr Address END_OFFSET = 1024;
    std::string str;
    const Address sp = read_sp(*cpu);
    for (Address addr = sp - START_OFFSET; addr <= sp + END_OFFSET; addr += 4) {
        if (Ptr<uint32_t>(addr).valid(mem)) {
            const Address value = *Ptr<uint32_t>(addr).get(mem);
            const auto mod = kernel.find_module_by_addr(value);
            if (mod)
                fmt::format_to(std::back_inserter(str), "0x{:X} (module: {})\n", value, mod->module_name);
        }
    }
    return str;
}
