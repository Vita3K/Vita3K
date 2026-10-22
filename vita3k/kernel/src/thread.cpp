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
#include <kernel/sync_primitives.h>
#include <mem/ptr.h>
#include <util/align.h>

#include <util/log.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>
#include <utility>

// Set in the wait type of a wait that runs callbacks
constexpr SceUInt32 WAITTYPE_CB_BIT = 0x80000000U;
// What a thread waiting for a thread signal reports as its wait UID on a Vita, which isn't a UID
constexpr SceUID SIGNAL_WAIT_ID = 0x00010221;

// The priority a thread created with priority runs at. The default range stands for the game's own.
static int created_priority(int priority) {
    if (priority > SCE_KERNEL_LOWEST_PRIORITY_USER) {
        assert(SCE_KERNEL_HIGHEST_DEFAULT_PRIORITY <= priority && priority <= SCE_KERNEL_LOWEST_DEFAULT_PRIORITY);
        return priority - SCE_KERNEL_DEFAULT_PRIORITY + SCE_KERNEL_GAME_DEFAULT_PRIORITY_ACTUAL;
    }
    return priority;
}

// The size of the kernel's part of a thread's TLS, below the part the guest sees
constexpr size_t KERNEL_TLS_SIZE = 0x800;

// The registers each run of a thread starts with
static CPUContext initial_context(Address stack_top, const SceKernelThreadOptParam *option) {
    CPUContext ctx;
    // A Vita starts every thread with these in r2 to r12, and some games rely on r9 not being 0
    std::fill(ctx.cpu_registers.begin() + 2, ctx.cpu_registers.begin() + 13, 0xDEADBEEF);
    ctx.set_sp(stack_top);
    ctx.fpscr = INITIAL_FPSCR;
    if (option) {
        ctx.cpu_registers[0] = option->attr;
        ctx.cpu_registers[1] = option->size;
    }
    return ctx;
}

ThreadState::ThreadState(SceUID id, KernelState &kernel, MemState &mem, CPUStatePtr cpu, const char *name, Address entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr, const SceKernelThreadOptParam *option)
    : id(id)
    , name(name)
    , entry_point(entry_point)
    , attr(attr | THREAD_ATTR_USER)
    , stack_size(align(stack_size, KiB(4)))
    , stack(alloc_block(mem, this->stack_size, fmt::format("Stack for thread {} (#{})", name, id).c_str()))
    , tls(alloc_block(mem, KERNEL_TLS_SIZE + kernel.tls_msize, fmt::format("TLS for thread {} (#{})", name, id).c_str()))
    , cpu(std::move(cpu))
    , init_priority(created_priority(init_priority))
    , init_affinity_mask(affinity_mask)
    , init_cpu_ctx(initial_context(stack.get() + this->stack_size, option))
    , kernel(kernel)
    , mem(mem)
    , priority(this->init_priority)
    , affinity_mask(affinity_mask) {
    if (kernel.debugger.watch_code) {
        set_log_code(*this->cpu, true);
    }
    if (kernel.debugger.watch_memory) {
        set_log_mem(*this->cpu, true);
    }

    memset(stack.get_ptr<void>().get(mem), 0xcc, this->stack_size);

    const size_t tls_size = KERNEL_TLS_SIZE + kernel.tls_msize;
    const Ptr<uint8_t> base_tls_ptr = tls.get_ptr<uint8_t>();
    memset(base_tls_ptr.get(mem), 0, tls_size);

    int *tls_array = tls.get_ptr<int>().get(mem);

    tls_array[TLS_THREAD_ID] = id;
    tls_array[TLS_SP_TOP] = stack.get() + this->stack_size;
    tls_array[TLS_SP_BOTTOM] = stack.get();
    // A Vita starts every thread with a different random value in each of these. These are the
    // values one thread had on a PS Vita TV, the same for every thread so runs repeat.
    tls_array[TLS_RESERVED_6] = static_cast<int>(0xFEFA7314);
    tls_array[TLS_STACK_CHECK_GUARD] = static_cast<int>(0xC325C2A7);
    tls_array[TLS_CURRENT_PRIORITY] = priority;
    tls_array[TLS_CPU_AFFINITY_MASK] = affinity_mask;

    const Ptr<uint8_t> user_tls_ptr = base_tls_ptr + KERNEL_TLS_SIZE;
    write_tpidruro(*this->cpu, user_tls_ptr.address());
    if (kernel.tls_address) {
        assert(kernel.tls_psize <= kernel.tls_msize);
        memcpy(user_tls_ptr.get(mem), kernel.tls_address.get(mem), kernel.tls_psize);
    }
}

int ThreadState::start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback) {
    return start_at(entry_point, arglen, argp, run_entry_callback);
}

int ThreadState::start_at(Address entry, SceSize arglen, const Ptr<void> argp, bool run_entry_callback) {
    std::unique_lock<std::mutex> thread_lock(mutex);
    if (status != ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_NOT_DORMANT;

    run_start_callback = run_entry_callback;
    returned_value = SCE_KERNEL_ERROR_NOT_DORMANT;
    // Each run starts at the priority and on the cores the thread was created with
    set_priority(init_priority);
    set_affinity_mask(init_affinity_mask);
    load_context(*cpu, init_cpu_ctx);
    // A thread starts with TPIDRURW at 0, also when it starts again
    write_tpidrurw(*cpu, 0);
    write_pc(*cpu, entry);
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

    status = ThreadStatus::running;
    status_cond.notify_one();

    return SCE_KERNEL_OK;
}

void ThreadState::exit(SceInt32 status) {
    std::lock_guard<std::mutex> guard(mutex);
    request_ending(Ending::exit);
    returned_value = static_cast<uint32_t>(status);
}

void ThreadState::exit_delete(SceInt32 status) {
    std::lock_guard<std::mutex> lock(mutex);
    request_ending(Ending::exit_delete);
    returned_value = static_cast<uint32_t>(status);
}

void ThreadState::remove() {
    const std::lock_guard<std::mutex> lock(mutex);
    request_removal();
}

SceInt32 ThreadState::remove_dormant() {
    const std::lock_guard<std::mutex> lock(mutex);
    if (status != ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    request_removal();
    return SCE_KERNEL_OK;
}

void ThreadState::request_removal() {
    request_ending(Ending::remove);
    // A dormant thread is gone at once
    if (status == ThreadStatus::dormant)
        update_status(ThreadStatus::deleted);

    if (status == ThreadStatus::running)
        stop(*cpu);

    // run_loop() may wait on status_cond, parked dormant or held at its gate
    status_cond.notify_all();

    // Wake if blocked in a wait
    wait_cv.notify_all();
}

void ThreadState::run_loop(bool top_level) {
    bool guest_returned = false;

    // Set thread-local CPU state so signal handlers can access it. The guard puts back what was
    // there before on any exit, so a nested run_loop for a callback leaves the outer one's state
    // set, and a recycled host thread never sees a stale CPUState pointer.
    struct CpuStateGuard {
        CPUState *const previous = get_current_cpu_state();
        ~CpuStateGuard() { set_current_cpu_state(previous); }
    } cpu_state_guard;
    set_current_cpu_state(cpu.get());

    std::unique_lock<std::mutex> lock(mutex);

    auto run_thread_end_callback = [&]() {
        if (!kernel.thread_event_end)
            return;

        const ThreadStatus old_status = status;
        const uint32_t old_returned_value = returned_value;
        // The handler runs guest code on this thread, which a pending ending would end at once
        const Ending old_ending = std::exchange(ending, Ending::none);
        status = ThreadStatus::running;

        lock.unlock();
        // A thread event handler starts with the VFP state of a new thread, like a callback
        const int ret = run_callback(kernel.thread_event_end.address(), { SCE_KERNEL_THREAD_EVENT_TYPE_END, static_cast<uint32_t>(id), 0, kernel.thread_event_end_arg }, true);
        if (ret != 0)
            LOG_WARN("Thread end event handler returned {}", log_hex(ret));
        lock.lock();

        status = old_status;
        returned_value = old_returned_value;
        // Keeping a stronger request that came while the handler ran, like a teardown's remove
        request_ending(old_ending);
    };

    // A mutex or a write lock the thread still holds when it ends goes to its next waiter, and the
    // event flags it holds are free again
    auto release_locks = [&]() {
        lock.unlock();
        kernel.objects.for_each<HeavyMutex>([&](Mutex &mutex) { mutex.release_ended_owner(*this); });
        kernel.objects.for_each<RWLock>([&](RWLock &rwlock) { rwlock.release_ended_writer(*this); });
        kernel.objects.for_each<EventFlag>([&](EventFlag &flag) { flag.release_ended_holder(*this); });
        lock.lock();
    };

    // The callbacks the thread created end with it, with their pending notifications
    auto delete_callbacks = [&]() {
        lock.unlock();
        for (const auto &weak_cb : callbacks) {
            if (const CallbackPtr cb = weak_cb.lock())
                kernel.delete_callback(cb->uid);
        }
        callbacks.clear();
        lock.lock();
    };

    while (true) {
        // This level is done once its guest function returned or the thread is ending
        if (exiting() || guest_returned) {
            // A nested level returns to the one below it
            if (!top_level)
                break;
            // A thread that returns from its entry ends like one that exits, end handler included
            if (guest_returned)
                request_ending(Ending::exit);
            // The top level ends a thread that ran. An exit leaves it dormant, while a delete makes
            // it gone for guest calls before its end waiters wake.
            if (status != ThreadStatus::dormant && status != ThreadStatus::deleted) {
                if (ending != Ending::remove)
                    run_thread_end_callback();
                release_locks();
                delete_callbacks();
                update_status(ending == Ending::exit ? ThreadStatus::dormant : ThreadStatus::deleted);
            }
            if (ending != Ending::exit)
                break;
            ending = Ending::none;
            guest_returned = false;
            // Status is now dormant: we'll park until the next start() or a delete
        }

        // Park until we have something to do.
        if (status != ThreadStatus::running) {
            status_cond.wait(lock, [&] { return status == ThreadStatus::running || exiting(); });
            continue;
        }

        // Fire the thread-start event once per start.
        if (run_start_callback) {
            run_start_callback = false;
            lock.unlock();
            if (kernel.thread_event_start) {
                // A thread event handler starts with the VFP state of a new thread, like a callback
                const int ret = run_callback(kernel.thread_event_start.address(), { SCE_KERNEL_THREAD_EVENT_TYPE_START, static_cast<uint32_t>(id), 0, kernel.thread_event_start_arg }, true);
                if (ret != 0)
                    LOG_WARN("Thread start event handler returned {}", log_hex(ret));
            }
            lock.lock();
        }

        // Active JIT loop. Lock held on entry and exit; unlocked only around run/step. A nested
        // level runs a callback or handler from inside a kernel call.
        StopPoint last_stop = top_level ? StopPoint::guest_code : StopPoint::syscall;
        while (!exiting() && !guest_returned && status == ThreadStatus::running) {
            wait_while_held(lock, last_stop);
            if (exiting())
                break;
            const bool do_step = std::exchange(step_requested, false);

            lock.unlock();

            // Single step or run
            const int res = do_step ? step(*cpu) : run(*cpu);

            // handle svc call if this was what stopped the cpu
            const bool syscall = cpu->svc_called;
            if (syscall) {
                const uint32_t nid = *Ptr<uint32_t>(read_pc(*cpu) + 4).get(mem);
                kernel.call_import(*cpu, nid, id);
                clear_exclusive(*cpu);
            }

            // handle pending abort (exception handler from page fault)
            if (cpu->abort_pending.exchange(false))
                dispatch_abort(*cpu);

            lock.lock();

            last_stop = syscall ? StopPoint::syscall : StopPoint::guest_code;
            // A finished step or a breakpoint holds the thread for the debugger
            if (do_step || hit_breakpoint(*cpu))
                holds.debugger = true;

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
}

// What a ForVM resume returns for a thread no ForVM suspend holds, an error with no public name
constexpr int VM_NOT_SUSPENDED = static_cast<int>(0x80028033);

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

uint32_t ThreadState::run_callback(Address callback_address, const std::vector<uint32_t> &args, bool fresh_vfp_state) {
    std::unique_lock<std::mutex> thread_lock(mutex);

    // save the current context before overwriting PC/LR for the callback
    const CPUContext previous_ctx = save_context(*cpu);
    const uint32_t previous_tpidruro = read_tpidruro(*cpu);
    // A callback sees the thread's TPIDRURW, and the thread gets its own value back after it
    const uint32_t previous_tpidrurw = read_tpidrurw(*cpu);
    const uint32_t previous_returned_value = returned_value;
    int &vfp_exception = tls.get_ptr<int>().get(mem)[TLS_VFP_EXCEPTION];
    const int previous_vfp_exception = vfp_exception;
    if (fresh_vfp_state) {
        write_fpscr(*cpu, INITIAL_FPSCR);
        vfp_exception = 0;
    }

    // we shouldn't have to clean the context I believe
    write_pc(*cpu, callback_address);
    write_lr(*cpu, kernel.halt_instruction_pc);
    push_arguments(args);
    thread_lock.unlock();

    // unlock but then immediately lock back in the run_loop function
    // shouldn't cause an issue, but maybe we could use a recursive mutex instead
    run_loop(false);

    thread_lock.lock();

    // restore the previous context
    // actually, in most case I don't think this is necessary as the caller
    // and the callee should respect the same calling convention
    // but do it just in case
    load_context(*cpu, previous_ctx);
    write_tpidruro(*cpu, previous_tpidruro);
    write_tpidrurw(*cpu, previous_tpidrurw);
    if (fresh_vfp_state)
        vfp_exception = previous_vfp_exception;

    // The callback's result isn't the thread's exit status, unless the callback ended the thread
    const uint32_t result = returned_value;
    if (!exiting())
        returned_value = previous_returned_value;
    return result;
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
    start_at(callback_address, args, argp, false);
    std::unique_lock<std::mutex> lock(mutex);
    status_cond.wait(lock, [&]() { return status == ThreadStatus::dormant || status == ThreadStatus::deleted; });
    return returned_value;
}

void ThreadState::update_status(ThreadStatus status, std::optional<ThreadStatus> expected) {
    if (expected)
        assert(expected.value() == this->status);

    this->status = status;
    status_cond.notify_all();

    if (status == ThreadStatus::dormant || status == ThreadStatus::deleted) {
        const std::lock_guard<std::mutex> end_lock(end_waiters_mutex);
        end_waiters.wake_if([&](auto &waiter) {
            if (waiter.entry.exit_status)
                *waiter.entry.exit_status = static_cast<SceInt32>(returned_value);
            return true;
        });
    }
}

WaitResult ThreadState::delay_until(Deadline deadline, bool callbacks) {
    while (true) {
        WaitResult r = wait({ .type = SCE_KERNEL_WAITTYPE_DELAY, .callbacks = callbacks }, deadline);
        if (r && *r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            r = run_callbacks();
        if (!r)
            return r;
        // Reaching the deadline is the expected outcome of a delay
        if (*r == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
            return SCE_KERNEL_OK;
    }
}

WaitResult ThreadState::wait_for_signal(SceUInt32 *timeout, bool callbacks) {
    const auto take_signal = [this] {
        const std::lock_guard<std::mutex> lock(mutex);
        return std::exchange(signal_pending, false);
    };
    // A signal sent before ends the wait at once, with the timeout untouched
    if (take_signal())
        return SCE_KERNEL_OK;
    const Deadline deadline = deadline_from(timeout);
    WaitResult r = SCE_KERNEL_OK;
    while (!take_signal()) {
        r = wait({ SCE_KERNEL_WAITTYPE_SIGNAL, SIGNAL_WAIT_ID, callbacks }, deadline);
        if (r && *r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            r = run_callbacks();
        if (!r || *r != SCE_KERNEL_OK)
            break;
    }
    writeback_timeout(timeout, deadline);
    return r;
}

SceInt32 ThreadState::send_signal() {
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if (signal_pending)
            return SCE_KERNEL_ERROR_ALREADY_SENT;
        signal_pending = true;
    }
    wake();
    return SCE_KERNEL_OK;
}

bool ThreadState::is_deleted() {
    const std::lock_guard<std::mutex> lock(mutex);
    return status == ThreadStatus::deleted;
}

ThreadStatus ThreadState::get_status() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return status;
}

std::expected<uint32_t, SceInt32> ThreadState::exit_status() const {
    const std::lock_guard<std::mutex> lock(mutex);
    if (status != ThreadStatus::dormant)
        return std::unexpected(SCE_KERNEL_ERROR_NOT_DORMANT);
    // A thread that never started has no exit status yet
    if (returned_value == static_cast<uint32_t>(SCE_KERNEL_ERROR_DORMANT))
        return std::unexpected(SCE_KERNEL_ERROR_DORMANT);
    return returned_value;
}

int ThreadState::stack_free_size() const {
    const std::lock_guard<std::mutex> lock(mutex);
    // A dormant thread's whole stack is free, a started one's is free below its stack pointer
    if (status == ThreadStatus::dormant)
        return stack_size;
    return static_cast<int>(read_sp(*cpu) - stack.get());
}

void ThreadState::begin_host_wait() {
    const std::lock_guard<std::mutex> lock(mutex);
    update_status(ThreadStatus::waiting);
}

void ThreadState::end_host_wait() {
    const std::lock_guard<std::mutex> lock(mutex);
    update_status(ThreadStatus::running);
}

uint64_t ThreadState::next_display_wait(uint64_t next_vblank, int vcount) {
    const std::lock_guard<std::mutex> lock(mutex);
    last_vblank_waited = std::max(next_vblank, last_vblank_waited + vcount);
    return last_vblank_waited;
}

WaitResult ThreadState::wait_for_thread_end(const ThreadStatePtr &waiter, SceInt32 *exit_status, Deadline deadline, bool callbacks) {
    std::unique_lock<std::mutex> lock(mutex);
    if (status == ThreadStatus::dormant || status == ThreadStatus::deleted) {
        if (exit_status)
            *exit_status = static_cast<SceInt32>(returned_value);
        return SCE_KERNEL_OK;
    }
    std::unique_lock<std::mutex> end_lock(end_waiters_mutex);
    lock.unlock();
    return end_waiters.wait(end_lock, waiter, { SCE_KERNEL_WAITTYPE_WAITTHEND, id, callbacks }, { exit_status }, deadline);
}

WaitResult ThreadState::wait(WaitTarget target, Deadline deadline) {
    std::unique_lock<std::mutex> lock(mutex);
    const auto woken = [&] { return wait_over(); };
    wait_target = target;
    update_status(ThreadStatus::waiting);
    bool satisfied = true;
    if (deadline == Deadline::max())
        wait_cv.wait(lock, woken);
    else
        satisfied = wait_cv.wait_until(lock, deadline, woken);
    // A thread something holds goes on to its gate, which keeps it from its callbacks and its code
    update_status(ThreadStatus::running);
    const bool callbacks_due = callback_ends_wait();
    wait_target = {};
    if (exiting())
        return std::unexpected{ ThreadExiting{} };
    wake_pending = false;
    if (callbacks_due)
        return SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
    return satisfied ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_WAIT_TIMEOUT;
}

WaitResult ThreadState::run_callbacks() {
    process_callbacks();
    const std::lock_guard<std::mutex> lock(mutex);
    // The thread exited or was deleted during the callbacks, so the wait must not complete
    if (exiting())
        return std::unexpected{ ThreadExiting{} };
    // The callbacks may have changed what the caller waits for, so it rechecks before waiting again
    return SCE_KERNEL_OK;
}

void ThreadState::wake() {
    const std::lock_guard<std::mutex> lock(mutex);
    wake_pending = true;
    wait_cv.notify_all();
}

SceUInt32 ThreadState::process_callbacks() {
    {
        // Thread info reads the flag too, through wait_over()
        const std::lock_guard<std::mutex> lock(mutex);
        if (is_processing_callbacks)
            return 0;
        is_processing_callbacks = true;
    }
    SceUInt32 processed = 0;
    for (auto it = callbacks.begin(); it != callbacks.end();) {
        const CallbackPtr cb = it->lock();
        // Deleted since it was added
        if (!cb) {
            it = callbacks.erase(it);
            continue;
        }
        ++it;
        const std::optional<Callback::Notification> notification = cb->take_notification();
        if (!notification)
            continue;
        // A callback starts with the VFP state of a new thread, and the thread gets its own back after it
        const uint32_t ret = run_callback(cb->cb_func.address(),
            { static_cast<uint32_t>(notification->notifier_id), notification->count, static_cast<uint32_t>(notification->arg), cb->userdata.address() }, true);
        ++processed;
        // A callback that exits the thread also ends the wait it runs in
        {
            const std::lock_guard<std::mutex> lock(mutex);
            // The thread's other callbacks are deleted when it ends, with their notifications
            if (exiting())
                break;
        }
        // A callback that returns nonzero deletes itself
        if (ret != 0)
            kernel.delete_callback(cb->uid);
    }
    const std::lock_guard<std::mutex> lock(mutex);
    is_processing_callbacks = false;
    return processed;
}

void ThreadState::add_notified_callback() {
    const std::lock_guard<std::mutex> lock(mutex);
    ++notified_callbacks;
    wait_cv.notify_all();
}

void ThreadState::remove_notified_callback() {
    const std::lock_guard<std::mutex> lock(mutex);
    if (notified_callbacks > 0)
        --notified_callbacks;
}

void ThreadState::add_callback(const CallbackPtr &cb) {
    callbacks.push_back(cb);
}

SceKernelThreadInfo ThreadState::info() {
    const std::lock_guard<std::mutex> lock(mutex);
    SceKernelThreadInfo info{};
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH);
    info.attr = attr;
    info.stack = Ptr<void>(stack.get());
    info.stackSize = stack_size;
    info.initPriority = init_priority;
    info.currentPriority = priority;
    info.initCpuAffinityMask = init_affinity_mask;
    info.currentCpuAffinityMask = affinity_mask;
    info.entry = SceKernelThreadEntry(entry_point);
    // A thread whose kernel wait is over is READY until it goes on, and waits on nothing, like on a
    // Vita, where the waker ends the wait. A host wait, like an audio output, has no target and
    // stays WAITING.
    const bool ready = status == ThreadStatus::waiting && wait_target.type != 0 && wait_over();
    const WaitTarget reported = ready ? WaitTarget{} : wait_target;
    info.status = ready ? SCE_KERNEL_THREAD_STATUS_READY : std::to_underlying(status);
    info.waitType = reported.callbacks ? reported.type | WAITTYPE_CB_BIT : reported.type;
    info.waitId = reported.id;
    info.exitStatus = returned_value;
    info.fNotifyCallback = callbacks_notified() ? 1 : 0;
    return info;
}

int ThreadState::get_priority() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return priority;
}

int ThreadState::change_priority(int new_priority) {
    const std::lock_guard<std::mutex> lock(mutex);
    const int old_priority = priority;
    set_priority(new_priority);
    return old_priority;
}

void ThreadState::set_priority(int new_priority) {
    priority = new_priority;
    tls.get_ptr<int>().get(mem)[TLS_CURRENT_PRIORITY] = new_priority;
}

SceInt32 ThreadState::get_affinity_mask() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return affinity_mask;
}

SceInt32 ThreadState::change_affinity_mask(SceInt32 new_affinity_mask) {
    const std::lock_guard<std::mutex> lock(mutex);
    const SceInt32 old_affinity_mask = affinity_mask;
    set_affinity_mask(new_affinity_mask);
    return old_affinity_mask;
}

void ThreadState::set_affinity_mask(SceInt32 new_affinity_mask) {
    affinity_mask = new_affinity_mask;
    tls.get_ptr<int>().get(mem)[TLS_CPU_AFFINITY_MASK] = new_affinity_mask;
}

Address ThreadState::stack_top() const {
    return stack.get() + stack_size;
}

void ThreadState::wait_while_held(std::unique_lock<std::mutex> &lock, StopPoint where) {
    if (!holds.any() || exiting())
        return;
    stop_point = where;
    // The holds can change while the thread waits, and with them what its status shows
    while (holds.any() && !exiting()) {
        const ThreadStatus shown = holds.shown() ? ThreadStatus::suspended : ThreadStatus::running;
        if (status != shown)
            update_status(shown);
        status_cond.wait(lock);
    }
    if (status != ThreadStatus::running)
        update_status(ThreadStatus::running);
}

void ThreadState::suspend() {
    const std::lock_guard<std::mutex> lock(mutex);
    holds.debugger = true;
    if (status == ThreadStatus::running)
        stop(*cpu);
    // A thread a pause already holds at its gate shows as suspended now
    status_cond.notify_all();
}

bool ThreadState::resume(bool step) {
    const std::lock_guard<std::mutex> lock(mutex);
    // A step happens now or not at all
    if (step && (holds.pause || holds.vm > 0))
        return false;
    holds.debugger = false;
    step_requested = step;
    status_cond.notify_all();
    return !holds.any();
}

void ThreadState::pause() {
    const std::lock_guard<std::mutex> lock(mutex);
    holds.pause = true;
    if (status == ThreadStatus::running)
        stop(*cpu);
}

void ThreadState::resume_from_pause() {
    const std::lock_guard<std::mutex> lock(mutex);
    holds.pause = false;
    status_cond.notify_all();
}

int ThreadState::vm_suspend() {
    std::unique_lock<std::mutex> lock(mutex);
    if (status == ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_DORMANT;
    const int count = ++holds.vm;
    // A thread a pause already holds at its gate shows as suspended now
    status_cond.notify_all();
    if (status == ThreadStatus::running) {
        stop(*cpu);
        // Like on a Vita, return once the thread has stopped, so its context holds still
        wait_for_vm_stop(lock);
    }
    return count;
}

void ThreadState::wait_for_vm_stop(std::unique_lock<std::mutex> &lock) {
    status_cond.wait(lock, [&] { return status != ThreadStatus::running || !vm_suspended() || exiting(); });
}

SceInt32 ThreadState::get_vm_context(CPUContext &context, uint32_t &tpidrurw, bool &in_kernel_call) {
    std::unique_lock<std::mutex> lock(mutex);
    // A thread whose wait timed out while suspended stops on its way back to its code
    wait_for_vm_stop(lock);
    if (!vm_suspended())
        return SCE_KERNEL_ERROR_NOT_SUSPENDED;
    context = save_context(*cpu);
    tpidrurw = read_tpidrurw(*cpu);
    in_kernel_call = in_wait();
    return SCE_KERNEL_OK;
}

void ThreadState::wait_until_stopped_for_debugger() {
    std::unique_lock<std::mutex> lock(mutex);
    status_cond.wait(lock, [&] { return stopped_for_debugger() || status == ThreadStatus::dormant || status == ThreadStatus::deleted; });
}

void ThreadState::wait_until_not_running() {
    std::unique_lock<std::mutex> lock(mutex);
    status_cond.wait(lock, [&] { return status != ThreadStatus::running; });
}

bool ThreadState::stopped_at_breakpoint() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return stopped_for_debugger() && hit_breakpoint(*cpu);
}

int ThreadState::vm_resume() {
    const std::lock_guard<std::mutex> lock(mutex);
    // Like on a Vita, a dormant thread fails before the count does
    if (status == ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_DORMANT;
    if (holds.vm == 0)
        return VM_NOT_SUSPENDED;
    if (--holds.vm > 0)
        return holds.vm;
    // The gate waits for this, and so may a suspend that waits for the thread to stop
    status_cond.notify_all();
    return 0;
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
