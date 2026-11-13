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

#include <cpu/state.h>
#include <kernel/thread/thread_state.h>

#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <mem/ptr.h>
#include <util/align.h>

#include <util/log.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#include <memory>
#include <utility>

// The priority a thread created with priority runs at. The default range stands for the game's own.
static int created_priority(int priority) {
    if (priority > SCE_KERNEL_LOWEST_PRIORITY_USER) {
        assert(SCE_KERNEL_HIGHEST_DEFAULT_PRIORITY <= priority && priority <= SCE_KERNEL_LOWEST_DEFAULT_PRIORITY);
        return priority - SCE_KERNEL_DEFAULT_PRIORITY + SCE_KERNEL_GAME_DEFAULT_PRIORITY_ACTUAL;
    }
    return priority;
}

// The registers each run of a thread starts with
static CPUContext initial_context(Address stack_top) {
    CPUContext ctx;
    // A Vita starts every thread with these in r2 to r12, and some games rely on r9 not being 0
    std::fill(ctx.cpu_registers.begin() + 2, ctx.cpu_registers.begin() + 13, 0xDEADBEEF);
    ctx.set_sp(stack_top);
    ctx.fpscr = SCE_KERNEL_THREAD_INITIAL_FPSCR;
    return ctx;
}

// Puts the fill value in the d registers the kernel doesn't keep when it resumes a thread
static void fill_unkept_vfp(CPUContext &ctx) {
    for (std::size_t d = 0; d < 32; d++) {
        if (d == 2 || d == 3 || (d >= 8 && d <= 15))
            continue;
        ctx.fpu_registers[2 * d] = std::bit_cast<float>(SCE_KERNEL_VFP_REGISTER_FILL_LOW);
        ctx.fpu_registers[2 * d + 1] = std::bit_cast<float>(SCE_KERNEL_VFP_REGISTER_FILL_HIGH);
    }
}

ThreadState::ThreadState(SceUID id, KernelState &kernel, MemState &mem, CPUStatePtr cpu, const char *name, Address entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr)
    : WithUidClass(SCE_KERNEL_ATTR_TH_FIFO, name)
    , id(id)
    , entry_point(entry_point)
    , thread_attr(attr | SCE_KERNEL_THREAD_ATTR_USER)
    , stack_size(align(stack_size, KiB(4)))
    , stack(alloc_block(mem, this->stack_size, fmt::format("Stack for thread {} (#{})", name, id).c_str()))
    , tls(alloc_block(mem, SCE_KERNEL_TLS_SIZE + kernel.tls_msize, fmt::format("TLS for thread {} (#{})", name, id).c_str()))
    , cpu(std::move(cpu))
    , init_priority(created_priority(init_priority))
    , init_affinity_mask(affinity_mask)
    , kernel(kernel)
    , mem(mem)
    , priority(this->init_priority)
    , affinity_mask(affinity_mask) {
    uid = id;
    memset(stack.get_ptr<void>().get(mem), 0xcc, this->stack_size);
    memset(tls.get_ptr<void>().get(mem), 0, SCE_KERNEL_TLS_SIZE + kernel.tls_msize);

    tls_slot(TLS_THREAD_ID) = id;
    tls_slot(TLS_SP_TOP) = stack.get() + this->stack_size;
    tls_slot(TLS_SP_BOTTOM) = stack.get();
    // A Vita starts every thread with a different random value in each of these. These are the
    // values one thread had on a PS Vita TV, the same for every thread so runs repeat.
    tls_slot(TLS_RESERVED_6) = static_cast<int>(0xFEFA7314);
    tls_slot(TLS_STACK_CHECK_GUARD) = static_cast<int>(0xC325C2A7);
    tls_slot(TLS_CURRENT_PRIORITY) = priority;
    tls_slot(TLS_CPU_AFFINITY_MASK) = affinity_mask;

    const Ptr<uint8_t> user_tls_ptr = tls.get_ptr<uint8_t>() + SCE_KERNEL_TLS_SIZE;
    this->cpu->set_tpidruro(user_tls_ptr.address());
    if (kernel.tls_address) {
        assert(kernel.tls_psize <= kernel.tls_msize);
        memcpy(user_tls_ptr.get(mem), kernel.tls_address.get(mem), kernel.tls_psize);
    }
}

ThreadState::~ThreadState() {
    assert(held_flags.empty() && held_mutexes.empty() && held_rwlocks.empty());
}

int ThreadState::start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback) {
    const std::lock_guard<std::mutex> thread_lock(thread_mutex);
    if (running_event_handlers)
        return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
    if (status != ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    assert(held_flags.empty() && held_mutexes.empty() && held_rwlocks.empty());
    run_start_callback = run_entry_callback;
    returned_value = SCE_KERNEL_ERROR_NOT_DORMANT;
    // Vita3K runs every thread as CPU 0, and a later start keeps the CPU fields
    if (last_cpu == -1) {
        last_cpu = 0;
        change_cpu_count = 0;
    }
    // A start clears a callback notification the thread's last run left, which its end keeps
    callback_pending = false;
    // Each run starts at the priority and on the cores the thread was created with, and its TLS
    // shows them
    priority = init_priority;
    affinity_mask = init_affinity_mask;
    tls_slot(TLS_CURRENT_PRIORITY) = priority;
    tls_slot(TLS_CPU_AFFINITY_MASK) = affinity_mask;
    // A thread starts with TPIDRURW at 0, also when it starts again
    CPUContext ctx = initial_context(stack.get() + stack_size);
    // The entry gets the size of the argument data and its copy on the stack, or null for none
    Address data_addr = 0;
    if (argp && arglen > 0) {
        data_addr = ctx.get_sp() - align(arglen, 8);
        ctx.set_sp(data_addr);
        memcpy(Ptr<uint8_t>(data_addr).get(mem), argp.get(mem), arglen);
    }
    ctx.place_call(mem, entry_point, cpu->return_pc, { arglen, data_addr });
    cpu->set_context(ctx);

    update_status(ThreadStatus::running);
    return SCE_KERNEL_OK;
}

void ThreadState::exit(SceInt32 status) {
    request_exit(Ending::exit, status);
}

void ThreadState::exit_delete(SceInt32 status) {
    request_exit(Ending::exit_delete, status);
}

void ThreadState::request_exit(Ending how, SceInt32 status) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    if (callback_depth > 0 || running_event_handlers) {
        lock.unlock();
        LOG_CRITICAL("Thread {} exited inside a callback or a thread event handler, ending the app", id);
        kernel.request_process_exit(0);
        return;
    }
    request_ending(how);
    returned_value = static_cast<uint32_t>(status);
}

void ThreadState::remove() {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    request_ending(Ending::remove);
    // A dormant thread is gone at once
    if (status == ThreadStatus::dormant)
        update_status(ThreadStatus::deleted);
    else if (status == ThreadStatus::running)
        cpu->request_stop();

    // Wake the thread if it is parked dormant, held before its guest code or blocked in a wait
    cv.notify_all();
}

SceInt32 ThreadState::remove_dormant() {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    if (status != ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    // A dormant thread is gone at once, and the status change wakes its parked host thread to end
    request_ending(Ending::remove);
    update_status(ThreadStatus::deleted);
    return SCE_KERNEL_OK;
}

namespace {
// Sets the current CPU state of the host thread for signal handlers, and puts the previous one
// back at the end of the scope
struct CurrentCpuState {
    CPUState *const previous = get_current_cpu_state();
    explicit CurrentCpuState(CPUState &state) { set_current_cpu_state(&state); }
    ~CurrentCpuState() { set_current_cpu_state(previous); }
};
} // namespace

void ThreadState::run_loop() {
    // Signal handlers find the thread's CPU for the whole host thread, also while the thread ends
    // outside run_guest
    const CurrentCpuState current_cpu(*cpu);

    std::unique_lock<std::mutex> lock(thread_mutex);
    do {
        // A thread that returns from its entry ends like one that exits, end handler included
        if (const std::optional<uint32_t> returned = run_guest(lock, false)) {
            returned_value = *returned;
            request_ending(Ending::exit);
        }
    } while (finish_run(lock));
}

bool ThreadState::finish_run(std::unique_lock<std::mutex> &lock) {
    // The thread is ending. One that ran ends here: an exit leaves it dormant, while a delete
    // makes it gone for guest calls before its end waiters wake.
    if (status != ThreadStatus::dormant && status != ThreadStatus::deleted) {
        if (ending != Ending::remove) {
            // The thread is RUNNING here, since wait() puts it back to RUNNING. The END handlers run
            // guest code on it, which a pending ending would end at once. They see NOT_DORMANT as
            // the exit status, and an exit in a handler doesn't change it.
            const uint32_t exit_status = std::exchange(returned_value, SCE_KERNEL_ERROR_NOT_DORMANT);
            const Ending old_ending = ending;
            set_ending(Ending::none);
            run_event_handlers(lock, SCE_KERNEL_THREAD_EVENT_TYPE_END);
            returned_value = exit_status;
            // Keep a stronger request made while the handlers ran, like a teardown's remove
            request_ending(old_ending);
        }

        // The event flags the thread holds are free again, and its mutexes and write locks go to
        // their next waiters. Each release unlists the object, unless a cancel or a delete already did.
        while (const Ref<EventFlag> flag = held_flags.front()) {
            lock.unlock();
            flag->release_if_held_by(*this);
            lock.lock();
        }
        while (const Ref<Mutex> mutex = held_mutexes.front()) {
            lock.unlock();
            mutex->release_if_owned_by(*this);
            lock.lock();
        }
        while (const Ref<RWLock> rwlock = held_rwlocks.front()) {
            lock.unlock();
            rwlock->release_if_write_locked_by(*this);
            lock.lock();
        }
        // The callbacks the thread created end with it, with their pending notifications
        while (!callbacks.empty())
            close_callback(*callbacks.begin());
        lock.unlock();
        // The end waiters see the status and the exit status together. Both locks stay held until
        // they are woken, so a start can't come in between.
        const auto end_guard = this->lock();
        lock.lock();
        update_status(ending == Ending::exit ? ThreadStatus::dormant : ThreadStatus::deleted);
        waiters.wake_if([&](auto &waiter) {
            if (waiter.wait_request().out_status)
                *waiter.wait_request().out_status = static_cast<SceInt32>(returned_value);
            return true;
        });
    }
    if (ending != Ending::exit)
        return false;
    // The thread is dormant now, and the next run_guest parks it until a start or a delete
    set_ending(Ending::none);
    return true;
}

std::optional<uint32_t> ThreadState::run_guest(std::unique_lock<std::mutex> &lock, bool from_kernel_call) {
    // Guest code runs with this CPU as the current CPU state, and a nested call puts the outer one
    // back when it returns
    const CurrentCpuState current_cpu(*cpu);
    // Whether this level's guest code last stopped right after a kernel call, which a ForVM hold
    // before its next guest code reports. Each level has its own, a nested call doesn't change it.
    bool after_kernel_call = from_kernel_call;
    while (true) {
        // Park until the thread runs: dormant between starts, or busy elsewhere for a call that
        // another host thread makes
        cv.wait(lock, [&] { return status == ThreadStatus::running || exiting(); });
        if (exiting())
            return std::nullopt;

        // The START handlers run once per start
        if (std::exchange(run_start_callback, false))
            run_event_handlers(lock, SCE_KERNEL_THREAD_EVENT_TYPE_START);

        // After the start event, which runs as a nested call, so this level's own value is published
        stopped_after_kernel_call = after_kernel_call;
        stop_for_holds(lock);
        if (exiting())
            return std::nullopt;
        const bool step = step_requested;

        lock.unlock();
        const GuestExit exit = cpu->run(step ? CPUState::RunMode::single_step : CPUState::RunMode::run);
        if (step) {
            lock.lock();
            // A stop request made before the step ends it before its instruction, so the step goes
            // again after the holds, unless the debugger lets the thread go meanwhile
            if (exit.kind == GuestExit::Kind::stopped && !exit.debugger_stop)
                continue;
            // The step is taken, so a call that a stepped import makes doesn't step too
            step_requested = false;
            lock.unlock();
        }

        // In this order, also when the import ended the thread: the import, the exclusive monitor,
        // the abort handler, then the debugger's hold
        switch (exit.kind) {
        case GuestExit::Kind::import:
            cpu->write_call_result(kernel.call_import(*cpu, exit.value, id));
            cpu->clear_exclusive();
            after_kernel_call = true;
            break;
        case GuestExit::Kind::stopped:
            after_kernel_call = false;
            break;
        case GuestExit::Kind::returned:
            break;
        case GuestExit::Kind::fault:
            // A fault in guest code ends the app, and the loop top then ends this thread
            kernel.request_process_exit(0);
            break;
        }
        // A host fault on protected guest memory, in guest code or in the import's host code
        if (const std::optional<Address> fault = cpu->take_abort())
            dispatch_abort(*fault);
        lock.lock();
        if (exit.debugger_stop)
            debugger_hold = true;
        if (exit.kind == GuestExit::Kind::returned)
            return exit.value;
        // A thread that doesn't run here anymore parks again at the top
    }
}

std::optional<uint32_t> ThreadState::enter_guest(std::unique_lock<std::mutex> &lock, CPUContext ctx, Address fn, std::initializer_list<uint32_t> args) {
    ctx.place_call(mem, fn, cpu->return_pc, args);
    cpu->set_context(ctx);
    // A call runs inside a kernel call, so a hold before its first instruction finds the thread there
    return run_guest(lock, true);
}

std::optional<uint32_t> ThreadState::call_guest(Address fn, std::initializer_list<uint32_t> args) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    const CPUContext caller = cpu->get_context();
    const std::optional<uint32_t> returned = enter_guest(lock, caller, fn, args);
    // A plain function call returns with the registers the function leaves. Only the return point
    // place_call replaced comes back: PC, LR, SP below the stack arguments, and the CPSR's state.
    CPUContext ctx = cpu->get_context();
    ctx.set_sp(caller.get_sp());
    ctx.set_lr(caller.get_lr());
    ctx.set_pc(caller.get_pc() | caller.thumb());
    ctx.cpsr = caller.cpsr;
    cpu->set_context(ctx);
    return returned;
}

std::optional<uint32_t> ThreadState::run_callback(Address fn, std::initializer_list<uint32_t> args) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    CPUContext saved = cpu->get_context();
    const uint32_t tpidruro = cpu->get_tpidruro();
    CPUContext entry = initial_context(saved.get_sp());
    // The callback sees the thread's TPIDRURW, and the thread gets its own value back after it
    entry.tpidrurw = saved.tpidrurw;
    const int vfp_exception = std::exchange(tls_slot(TLS_VFP_EXCEPTION), 0);
    const std::optional<uint32_t> returned = enter_guest(lock, entry, fn, args);
    // It returns through the kernel, which resumes the thread from the save it took before. d2 and
    // d3 are s4 to s7, which keep the callback's values.
    const CPUContext end = cpu->get_context();
    std::copy_n(end.fpu_registers.begin() + 4, 4, saved.fpu_registers.begin() + 4);
    fill_unkept_vfp(saved);
    cpu->set_context(saved);
    cpu->set_tpidruro(tpidruro);
    tls_slot(TLS_VFP_EXCEPTION) = vfp_exception;
    return returned;
}

void ThreadState::dispatch_abort(Address fault_addr) {
    CPUState &cpu = *this->cpu;
    // DABT = type 0
    const Address handler = kernel.exception_handlers[0].load();
    if (!handler)
        return;

    // The handler reads a KuKernelAbortContext on the stack: r0 to r12, sp, lr, pc and the fault
    // address. protect_tree already unprotected the page and the CPU may have run past the faulting
    // instruction, so the call is only a notification and nothing is restored from the context.
    const uint32_t ctx_size = 17 * 4;
    const uint32_t sp_orig = cpu.get_sp();
    const uint32_t sp_aligned = align_down(sp_orig - ctx_size, 8);

    auto *ctx = Ptr<uint32_t>(sp_aligned).get(cpu.mem);
    for (int i = 0; i < 13; i++)
        ctx[i] = cpu.get_reg(i);
    ctx[13] = sp_aligned + ctx_size;
    ctx[14] = cpu.get_lr();
    ctx[15] = cpu.get_pc();
    ctx[16] = fault_addr;

    LOG_DEBUG("DABT handler=0x{:08X} FAR=0x{:08X} PC=0x{:08X} SP=0x{:08X} sp_aligned=0x{:08X}",
        handler, fault_addr, ctx[15], sp_orig, sp_aligned);

    // The handler interrupts guest code, which goes on with all its registers
    const CPUContext interrupted = cpu.get_context();
    call_guest(handler, { sp_aligned });
    cpu.set_context(interrupted);
}

uint32_t ThreadState::start_and_wait(SceSize args, const Ptr<void> argp) {
    start(args, argp);
    std::unique_lock<std::mutex> lock(thread_mutex);
    cv.wait(lock, [&] { return status == ThreadStatus::dormant || status == ThreadStatus::deleted; });
    return returned_value;
}

void ThreadState::update_status(ThreadStatus status) {
    this->status = status;
    cv.notify_all();
}

SceInt32 ThreadState::delay_until(Deadline deadline, bool callbacks) {
    SceInt32 r = callbacks ? run_callbacks() : SCE_KERNEL_OK;
    // A callback pending again after the first pass ends the delay at once
    if (callbacks && has_pending_callbacks())
        return r;
    while (r == SCE_KERNEL_OK) {
        r = wait({ .type = wait_type(SCE_KERNEL_WAITTYPE_DELAY, callbacks) }, deadline);
        if (r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            r = run_callbacks();
    }
    // Reaching the deadline is the expected outcome of a delay
    return r == SCE_KERNEL_ERROR_WAIT_TIMEOUT ? SCE_KERNEL_OK : r;
}

SceInt32 ThreadState::wait_for_signal(SceUInt32 *timeout, bool callbacks) {
    const auto take_signal = [this] {
        const std::lock_guard<std::mutex> lock(thread_mutex);
        return std::exchange(signal_pending, false);
    };
    const Deadline deadline = deadline_from(timeout);
    SceInt32 r = callbacks ? run_callbacks() : SCE_KERNEL_OK;
    // A signal sent before ends the wait at once, with the timeout as it was
    while (r == SCE_KERNEL_OK && !take_signal()) {
        {
            // A signal wait sleeps with a callback pending, only a notify during the sleep ends it
            std::unique_lock<std::mutex> lock(thread_mutex);
            r = sleep(lock, { wait_type(SCE_KERNEL_WAITTYPE_SIGNAL, callbacks), SCE_KERNEL_SIGNAL_WAIT_ID }, deadline);
        }
        writeback_timeout(timeout, deadline);
        if (r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            r = run_callbacks();
    }
    return r;
}

SceInt32 ThreadState::send_signal() {
    {
        const std::lock_guard<std::mutex> lock(thread_mutex);
        if (signal_pending)
            return SCE_KERNEL_ERROR_ALREADY_SENT;
        signal_pending = true;
    }
    wake();
    return SCE_KERNEL_OK;
}

ThreadStatus ThreadState::get_status() const {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    return shown_status();
}

ThreadStatus ThreadState::shown_status() const {
    // A ForVM suspend shows once the thread has stopped for it, and the host's holds never show
    if (status == ThreadStatus::running && vm_suspend_count > 0 && parked)
        return ThreadStatus::suspended;
    return status;
}

void ThreadState::set_ending(Ending how) {
    ending = how;
    if (how != Ending::none)
        step_requested = false;
}

bool ThreadState::held() const {
    return !exiting() && (debugger_hold || vm_suspend_count > 0 || kernel.is_threads_paused());
}

void ThreadState::stop_for_holds(std::unique_lock<std::mutex> &lock) {
    if (!held())
        return;
    parked = true;
    cv.notify_all();
    // The holds can change while the thread waits
    cv.wait(lock, [&] { return !held(); });
    parked = false;
}

int ThreadState::stack_free_size() const {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    // A dormant thread's whole stack is free, a started one's is free below its stack pointer
    if (status == ThreadStatus::dormant)
        return stack_size;
    return static_cast<int>(cpu->get_sp() - stack.get());
}

ThreadState::HostWait::HostWait(ThreadState &thread, std::optional<WaitTarget> shown)
    : thread(thread) {
    const std::lock_guard<std::mutex> lock(thread.thread_mutex);
    thread.host_waiting = true;
    if (shown) {
        thread.wait_target = *shown;
        thread.update_status(ThreadStatus::waiting);
    } else {
        thread.cv.notify_all();
    }
}

ThreadState::HostWait::~HostWait() {
    const std::lock_guard<std::mutex> lock(thread.thread_mutex);
    thread.host_waiting = false;
    if (thread.status == ThreadStatus::waiting) {
        thread.wait_target = {};
        thread.update_status(ThreadStatus::running);
    }
}

SceInt32 ThreadState::wait_for_thread_end(ThreadState &waiter, SceInt32 *exit_status, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    // An ended run gives its exit status at once with the timeout as it was. No callback runs after
    // the wait, and a thread is never deleted from the object table.
    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_WAITTHEND, callbacks);
    const Deadline deadline = deadline_from(timeout);
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, waiter, callbacks))
            return r;
        {
            const std::lock_guard<std::mutex> thread_lock(thread_mutex);
            if (status == ThreadStatus::dormant || status == ThreadStatus::deleted) {
                if (exit_status)
                    *exit_status = static_cast<SceInt32>(returned_value);
                return SCE_KERNEL_OK;
            }
        }
        const SceInt32 result = Waitable::wait(waiters, guard, waiter, type, { .out_status = exit_status }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return result;
    }
}

SceInt32 ThreadState::wait(WaitTarget target, Deadline deadline) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    // A callback already pending ends a wait with callbacks at once, unless the deadline passed
    if (!wait_result && (target.type & SCE_KERNEL_WAITTYPE_CB_BIT) && callback_pending && std::chrono::steady_clock::now() < deadline)
        wait_result = SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
    return sleep(lock, target, deadline);
}

SceInt32 ThreadState::sleep(std::unique_lock<std::mutex> &lock, WaitTarget target, Deadline deadline) {
    const auto woken = [&] { return wait_over(); };
    wait_target = target;
    update_status(ThreadStatus::waiting);
    if (deadline == Deadline::max())
        cv.wait(lock, woken);
    else
        cv.wait_until(lock, deadline, woken);
    // Before the call writes its result to the registers. A thread something holds goes on to its
    // holds, which keep it from its callbacks and its code.
    update_status(ThreadStatus::running);
    wait_target = {};
    const std::optional<SceInt32> result = std::exchange(wait_result, std::nullopt);
    if (exiting())
        return SCE_KERNEL_ERROR_THREAD_STOPPED;
    return result.value_or(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
}

SceInt32 ThreadState::run_callbacks() {
    process_callbacks();
    const std::lock_guard<std::mutex> lock(thread_mutex);
    // The thread exited or was deleted during the callbacks, so the wait must not complete
    if (exiting())
        return SCE_KERNEL_ERROR_THREAD_STOPPED;
    // The callbacks may have changed what the caller waits for, so it rechecks before waiting again
    return SCE_KERNEL_OK;
}

void ThreadState::wake() {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    // The first result ends the wait, a later one is dropped
    if (!wait_result)
        wait_result = SCE_KERNEL_OK;
    cv.notify_all();
}

bool ThreadState::notify_callback(Callback &cb, SceInt32 arg, SceUID notifier_id) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    if (!cb.linked())
        return false;
    cb.pending.notifier_id = notifier_id;
    cb.pending.arg = arg;
    ++cb.pending.count;
    // Every notify reaches the owner, also one of a deleted callback that still runs, and ends a wait
    // with callbacks
    callback_pending = true;
    if ((wait_target.type & SCE_KERNEL_WAITTYPE_CB_BIT) && !wait_result)
        wait_result = SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
    cv.notify_all();
    return true;
}

void ThreadState::cancel_callback(Callback &cb) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    cb.pending = {};
}

Callback::Notification ThreadState::callback_notifications(const Callback &cb) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    return cb.pending;
}

SceUID ThreadState::add_callback(CallbackRef cb) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    callbacks.push_back(cb);
    return kernel.objects.add(std::move(cb), kernel.get_next_uid());
}

bool ThreadState::delete_callback(Callback &cb) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    if (std::exchange(cb.deleted, true))
        return false;
    // A run going on keeps the callback, and the end of its last run closes it
    if (cb.running_count == 0)
        close_callback(cb);
    return true;
}

void ThreadState::close_callback(Callback &cb) {
    // The list's reference keeps cb alive until its UID is gone
    const CallbackRef listed = callbacks.remove(cb);
    kernel.objects.remove<Callback>(cb.uid, id);
}

void ThreadState::add_held(Ref<EventFlag> flag) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    held_flags.push_back(std::move(flag));
}

void ThreadState::remove_held(EventFlag &flag) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    [[maybe_unused]] const Ref<EventFlag> listed = held_flags.remove(flag);
    assert(listed);
}

void ThreadState::add_held(Ref<Mutex> mutex) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    held_mutexes.push_back(std::move(mutex));
}

void ThreadState::remove_held(Mutex &mutex) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    [[maybe_unused]] const Ref<Mutex> listed = held_mutexes.remove(mutex);
    assert(listed);
}

void ThreadState::add_held(Ref<RWLock> rwlock) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    held_rwlocks.push_back(std::move(rwlock));
}

void ThreadState::remove_held(RWLock &rwlock) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    [[maybe_unused]] const Ref<RWLock> listed = held_rwlocks.remove(rwlock);
    assert(listed);
}

bool ThreadState::has_pending_callbacks() {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    return callback_pending;
}

SceUInt32 ThreadState::process_callbacks() {
    std::unique_lock<std::mutex> lock(thread_mutex);
    // The pass clears the flag first, so a notify during the pass leaves it set
    if (!std::exchange(callback_pending, false))
        return 0;
    SceUInt32 processed = 0;
    // The thread's callbacks in creation order
    for (auto it = callbacks.begin(); it != callbacks.end();) {
        Callback &cb = *it;
        if (cb.deleted || cb.pending.count == 0) {
            ++it;
            continue;
        }
        const Callback::Notification notification = std::exchange(cb.pending, {});
        // The run keeps the callback listed and its UID valid
        ++cb.running_count;
        ++callback_depth;
        lock.unlock();
        const std::optional<uint32_t> ret = run_callback(cb.cb_func.address(),
            { static_cast<uint32_t>(notification.notifier_id), notification.count, static_cast<uint32_t>(notification.arg), cb.userdata.address() });
        ++processed;
        lock.lock();
        --cb.running_count;
        --callback_depth;
        // An end of the app during the callback ends the pass, and the thread's end deletes its
        // callbacks with their notifications. A call cut short only comes with an end.
        const bool ending = exiting();
        // A nonzero return deletes the callback. A delete during the run takes effect once the last
        // run ends.
        if (!ending && *ret != 0)
            cb.deleted = true;
        // The next one is read after the run, since a pass inside the callback may have closed it
        ++it;
        if (cb.deleted && cb.running_count == 0)
            close_callback(cb);
        if (ending)
            break;
    }
    return processed;
}

bool ThreadState::in_event_handler() const {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    return running_event_handlers;
}

void ThreadState::run_event_handlers(std::unique_lock<std::mutex> &lock, SceUInt32 type) {
    lock.unlock();
    std::unique_lock<std::mutex> list_lock(kernel.thread_event_handlers_mutex);
    for (auto it = kernel.thread_event_handlers.begin(); it != kernel.thread_event_handlers.end();) {
        ThreadEventHandler &handler = *it;
        if (!handler.runs_for(id, type)) {
            ++it;
            continue;
        }
        // The run keeps the handler listed, and the list keeps it alive
        ++handler.runs;
        list_lock.unlock();
        // Set before the handler calls, so with no handler a start racing this one gets NOT_DORMANT
        lock.lock();
        running_event_handlers = true;
        lock.unlock();
        const std::optional<uint32_t> ret = run_callback(handler.handler, { type, static_cast<uint32_t>(id), 0, handler.common });
        list_lock.lock();
        --handler.runs;
        // A nonzero return deletes the handler, and its UID stays
        if (ret && *ret != 0)
            handler.deleted = true;
        ++it;
        if (handler.deleted && handler.runs == 0)
            kernel.thread_event_handlers.remove(handler);
    }
    list_lock.unlock();
    lock.lock();
    running_event_handlers = false;
}

SceKernelThreadInfo ThreadState::info() {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    SceKernelThreadInfo info{};
    info.processId = PROCESS_ID;
    std::ranges::copy(name, info.name);
    info.attr = thread_attr;
    info.stack = Ptr<void>(stack.get());
    info.stackSize = stack_size;
    info.initPriority = init_priority;
    info.currentPriority = priority;
    info.initCpuAffinityMask = init_affinity_mask;
    info.currentCpuAffinityMask = affinity_mask;
    info.entry = SceKernelThreadEntry(entry_point);
    // A thread whose kernel wait is over is READY until it goes on, and waits on nothing, like on a
    // Vita, where the waker ends the wait. A host wait ends with its host call and stays WAITING.
    const bool ready = status == ThreadStatus::waiting && !host_waiting && wait_over();
    const WaitTarget reported = ready ? WaitTarget{} : wait_target;
    info.status = ready ? SCE_KERNEL_THREAD_STATUS_READY : std::to_underlying(shown_status());
    info.waitType = reported.type;
    info.waitId = reported.id;
    info.exitStatus = returned_value;
    info.fNotifyCallback = callback_pending ? 1 : 0;
    info.currentCpuId = status == ThreadStatus::running ? last_cpu : -1;
    info.lastExecutedCpuId = last_cpu;
    info.changeCpuCount = change_cpu_count;
    return info;
}

int ThreadState::get_priority() const {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    return priority;
}

int ThreadState::change_priority(int new_priority) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    tls_slot(TLS_CURRENT_PRIORITY) = new_priority;
    return std::exchange(priority, new_priority);
}

SceInt32 ThreadState::change_affinity_mask(SceInt32 new_affinity_mask) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    tls_slot(TLS_CPU_AFFINITY_MASK) = new_affinity_mask;
    return std::exchange(affinity_mask, new_affinity_mask);
}

void ThreadState::set_suspended(bool suspended) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    debugger_hold = suspended;
    // A step the thread didn't take doesn't outlive the debugger's next call
    step_requested = false;
    if (suspended && runs())
        cpu->request_stop();
    cv.notify_all();
}

bool ThreadState::step() {
    std::unique_lock<std::mutex> lock(thread_mutex);
    // A pause or a ForVM suspend holds the thread, so it doesn't step. One that comes after this
    // check delays the step until it ends.
    if (kernel.is_threads_paused() || vm_suspend_count > 0)
        return false;
    // A thread with no run, or one that is ending, has no instruction to step, and the wait ends
    // once it is dormant or deleted
    step_requested = (status == ThreadStatus::running || status == ThreadStatus::waiting) && !exiting();
    debugger_hold = false;
    cv.notify_all();
    // The thread holds for the debugger again after the instruction
    cv.wait(lock, [&] { return (debugger_hold && parked) || status == ThreadStatus::dormant || status == ThreadStatus::deleted; });
    return true;
}

void ThreadState::wait_until_not_running() {
    std::unique_lock<std::mutex> lock(thread_mutex);
    cv.wait(lock, [&] { return !runs(); });
}

bool ThreadState::stopped_at_breakpoint() const {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    return debugger_hold && parked;
}

void ThreadState::pause_changed() {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    if (kernel.is_threads_paused() && runs())
        cpu->request_stop();
    cv.notify_all();
}

int ThreadState::vm_suspend() {
    std::unique_lock<std::mutex> lock(thread_mutex);
    if (status == ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_DORMANT;
    const int count = ++vm_suspend_count;
    if (runs())
        cpu->request_stop();
    cv.notify_all();
    // Like on a Vita, return once the thread has stopped, so its context holds still
    cv.wait(lock, [&] { return parked || status != ThreadStatus::running || vm_suspend_count == 0 || exiting(); });
    return count;
}

int ThreadState::vm_resume() {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    // Like on a Vita, a dormant thread fails before the count does
    if (status == ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_DORMANT;
    if (vm_suspend_count == 0)
        return SCE_KERNEL_ERROR_VM_NOT_SUSPENDED;
    // The thread's holds wait for the last resume, and so may a suspend that waits for it to stop
    if (--vm_suspend_count == 0)
        cv.notify_all();
    return vm_suspend_count;
}

SceInt32 ThreadState::get_vm_context(CPUContext &context, bool &in_kernel_call) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    // A thread whose wait timed out while suspended stops on its way back to its code
    cv.wait(lock, [&] { return parked || status != ThreadStatus::running || vm_suspend_count == 0 || exiting(); });
    if (vm_suspend_count == 0)
        return SCE_KERNEL_ERROR_NOT_SUSPENDED;
    context = cpu->get_context();
    // Inside a kernel call means waiting, or held on the way back from one
    in_kernel_call = status == ThreadStatus::waiting || (parked && stopped_after_kernel_call);
    return SCE_KERNEL_OK;
}
