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

ThreadState::ThreadState(SceUID id, KernelState &kernel, MemState &mem, CPUStatePtr cpu, const char *name, Address entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr)
    : id(id)
    , name(name)
    , entry_point(entry_point)
    , attr(attr | SCE_KERNEL_THREAD_ATTR_USER)
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

int ThreadState::start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback) {
    const std::lock_guard<std::mutex> thread_lock(mutex);
    if (status != ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    run_start_callback = run_entry_callback;
    returned_value = SCE_KERNEL_ERROR_NOT_DORMANT;
    // Each run starts at the priority and on the cores the thread was created with, and its TLS
    // shows them
    priority = init_priority;
    affinity_mask = init_affinity_mask;
    tls_slot(TLS_CURRENT_PRIORITY) = priority;
    tls_slot(TLS_CPU_AFFINITY_MASK) = affinity_mask;
    cpu->load_context(initial_context(stack.get() + stack_size));
    // A thread starts with TPIDRURW at 0, also when it starts again
    cpu->set_tpidrurw(0);
    // The entry gets the size of the argument data and its copy on the stack, or null for none
    Address data_addr = 0;
    if (argp && arglen > 0) {
        data_addr = cpu->stack_alloc(align(arglen, 8));
        memcpy(Ptr<uint8_t>(data_addr).get(mem), argp.get(mem), arglen);
    }
    cpu->place_call(entry_point, { arglen, data_addr });

    update_status(ThreadStatus::running);
    return SCE_KERNEL_OK;
}

void ThreadState::exit(SceInt32 status) {
    const std::lock_guard<std::mutex> lock(mutex);
    request_ending(Ending::exit);
    returned_value = static_cast<uint32_t>(status);
}

void ThreadState::exit_delete(SceInt32 status) {
    const std::lock_guard<std::mutex> lock(mutex);
    request_ending(Ending::exit_delete);
    returned_value = static_cast<uint32_t>(status);
}

void ThreadState::remove() {
    const std::lock_guard<std::mutex> lock(mutex);
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
    const std::lock_guard<std::mutex> lock(mutex);
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

    std::unique_lock<std::mutex> lock(mutex);
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
        if (ending != Ending::remove && kernel.thread_event_end) {
            // The thread is RUNNING here, since wait() puts it back to RUNNING. The handler runs
            // guest code on it, which a pending ending would end at once, and an exit in the
            // handler must not change the exit status.
            const uint32_t old_returned_value = returned_value;
            const Ending old_ending = ending;
            set_ending(Ending::none);
            lock.unlock();
            // A thread event handler starts with the VFP state of a new thread, like a callback
            const int ret = call_guest(kernel.thread_event_end.address(), { SCE_KERNEL_THREAD_EVENT_TYPE_END, static_cast<uint32_t>(id), 0, kernel.thread_event_end_arg }, true);
            if (ret != 0)
                LOG_WARN("Thread end event handler returned {}", log_hex(ret));
            lock.lock();
            returned_value = old_returned_value;
            // Keep a stronger request made while the handler ran, like a teardown's remove
            request_ending(old_ending);
        }

        lock.unlock();
        // A mutex or a write lock the thread still holds goes to its next waiter, and the
        // event flags it holds are free again
        kernel.objects.for_each<HeavyMutex>([&](Mutex &mutex) { mutex.release_ended_owner(*this); });
        kernel.objects.for_each<RWLock>([&](RWLock &rwlock) { rwlock.release_ended_writer(*this); });
        kernel.objects.for_each<EventFlag>([&](EventFlag &flag) { flag.release_ended_holder(*this); });
        // The callbacks the thread created end with it, with their pending notifications
        for (const auto &weak_cb : callbacks) {
            if (const CallbackPtr cb = weak_cb.lock())
                kernel.delete_callback(cb->uid, id);
        }
        callbacks.clear();
        lock.lock();
        update_status(ending == Ending::exit ? ThreadStatus::dormant : ThreadStatus::deleted);
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

        // The thread start event runs once per start, with the VFP state of a new thread
        if (std::exchange(run_start_callback, false) && kernel.thread_event_start) {
            lock.unlock();
            const int ret = call_guest(kernel.thread_event_start.address(), { SCE_KERNEL_THREAD_EVENT_TYPE_START, static_cast<uint32_t>(id), 0, kernel.thread_event_start_arg }, true);
            if (ret != 0)
                LOG_WARN("Thread start event handler returned {}", log_hex(ret));
            lock.lock();
        }

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

uint32_t ThreadState::call_guest(Address fn, std::initializer_list<uint32_t> args, bool fresh_vfp_state) {
    std::unique_lock<std::mutex> thread_lock(mutex);

    // Save the thread's context before the call overwrites PC and LR
    const CPUContext previous_ctx = cpu->save_context();
    const uint32_t previous_tpidruro = cpu->get_tpidruro();
    // A callback sees the thread's TPIDRURW, and the thread gets its own value back after it
    const uint32_t previous_tpidrurw = cpu->get_tpidrurw();
    int &vfp_exception = tls_slot(TLS_VFP_EXCEPTION);
    const int previous_vfp_exception = vfp_exception;
    if (fresh_vfp_state) {
        cpu->set_fpscr(SCE_KERNEL_THREAD_INITIAL_FPSCR);
        vfp_exception = 0;
    }

    cpu->place_call(fn, args);

    // A callback runs inside a kernel call, so a hold before its first instruction finds the
    // thread there
    const std::optional<uint32_t> returned = run_guest(thread_lock, true);

    // The caller and the callback should keep to the calling convention, but put everything back
    cpu->load_context(previous_ctx);
    cpu->set_tpidruro(previous_tpidruro);
    cpu->set_tpidrurw(previous_tpidrurw);
    if (fresh_vfp_state)
        vfp_exception = previous_vfp_exception;

    // A callback the thread's end cut short returns the exit status
    return returned.value_or(returned_value);
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

    call_guest(handler, { sp_aligned });
}

uint32_t ThreadState::start_and_wait(SceSize args, const Ptr<void> argp) {
    start(args, argp);
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return status == ThreadStatus::dormant || status == ThreadStatus::deleted; });
    return returned_value;
}

void ThreadState::update_status(ThreadStatus status) {
    this->status = status;
    cv.notify_all();

    if (status == ThreadStatus::dormant || status == ThreadStatus::deleted) {
        const std::lock_guard<std::mutex> end_lock(end_waiters_mutex);
        end_waiters.wake_if([&](auto &waiter) {
            if (waiter.entry.exit_status)
                *waiter.entry.exit_status = static_cast<SceInt32>(returned_value);
            return true;
        });
    }
}

SceInt32 ThreadState::delay_until(Deadline deadline, bool callbacks) {
    while (true) {
        SceInt32 r = wait({ .type = SCE_KERNEL_WAITTYPE_DELAY, .callbacks = callbacks }, deadline);
        if (r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            r = run_callbacks();
        // Reaching the deadline is the expected outcome of a delay
        if (r == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
            return SCE_KERNEL_OK;
        if (r != SCE_KERNEL_OK)
            return r;
    }
}

SceInt32 ThreadState::wait_for_signal(SceUInt32 *timeout, bool callbacks) {
    const auto take_signal = [this] {
        const std::lock_guard<std::mutex> lock(mutex);
        return std::exchange(signal_pending, false);
    };
    // A signal sent before ends the wait at once, with the timeout untouched
    if (take_signal())
        return SCE_KERNEL_OK;
    const Deadline deadline = deadline_from(timeout);
    SceInt32 r = SCE_KERNEL_OK;
    while (!take_signal()) {
        r = wait({ SCE_KERNEL_WAITTYPE_SIGNAL, SCE_KERNEL_SIGNAL_WAIT_ID, callbacks }, deadline);
        if (r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            r = run_callbacks();
        if (r != SCE_KERNEL_OK)
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

ThreadStatus ThreadState::get_status() const {
    const std::lock_guard<std::mutex> lock(mutex);
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
    const std::lock_guard<std::mutex> lock(mutex);
    // A dormant thread's whole stack is free, a started one's is free below its stack pointer
    if (status == ThreadStatus::dormant)
        return stack_size;
    return static_cast<int>(cpu->get_sp() - stack.get());
}

ThreadState::HostWait::HostWait(ThreadState &thread, std::optional<WaitTarget> shown)
    : thread(thread) {
    const std::lock_guard<std::mutex> lock(thread.mutex);
    thread.host_waiting = true;
    if (shown) {
        thread.wait_target = *shown;
        thread.update_status(ThreadStatus::waiting);
    } else {
        thread.cv.notify_all();
    }
}

ThreadState::HostWait::~HostWait() {
    const std::lock_guard<std::mutex> lock(thread.mutex);
    thread.host_waiting = false;
    if (thread.status == ThreadStatus::waiting) {
        thread.wait_target = {};
        thread.update_status(ThreadStatus::running);
    }
}

uint64_t ThreadState::next_display_wait(uint64_t next_vblank, int vcount) {
    const std::lock_guard<std::mutex> lock(mutex);
    last_vblank_waited = std::max(next_vblank, last_vblank_waited + vcount);
    return last_vblank_waited;
}

SceInt32 ThreadState::wait_for_thread_end(const ThreadStatePtr &waiter, SceInt32 *exit_status, Deadline deadline, bool callbacks) {
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

SceInt32 ThreadState::wait(WaitTarget target, Deadline deadline) {
    std::unique_lock<std::mutex> lock(mutex);
    const auto woken = [&] { return wait_over(); };
    wait_target = target;
    update_status(ThreadStatus::waiting);
    bool satisfied = true;
    if (deadline == Deadline::max())
        cv.wait(lock, woken);
    else
        satisfied = cv.wait_until(lock, deadline, woken);
    // Before the call writes its result to the registers. A thread something holds goes on to its
    // holds, which keep it from its callbacks and its code.
    update_status(ThreadStatus::running);
    const bool callbacks_due = callback_ends_wait();
    wait_target = {};
    if (exiting())
        return SCE_KERNEL_ERROR_THREAD_STOPPED;
    wake_pending = false;
    if (callbacks_due)
        return SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
    return satisfied ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_WAIT_TIMEOUT;
}

SceInt32 ThreadState::run_callbacks() {
    process_callbacks();
    const std::lock_guard<std::mutex> lock(mutex);
    // The thread exited or was deleted during the callbacks, so the wait must not complete
    if (exiting())
        return SCE_KERNEL_ERROR_THREAD_STOPPED;
    // The callbacks may have changed what the caller waits for, so it rechecks before waiting again
    return SCE_KERNEL_OK;
}

void ThreadState::wake() {
    const std::lock_guard<std::mutex> lock(mutex);
    wake_pending = true;
    cv.notify_all();
}

bool ThreadState::has_pending_callbacks() {
    const std::lock_guard<std::mutex> lock(mutex);
    return !is_processing_callbacks && callbacks_notified();
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
        const uint32_t ret = call_guest(cb->cb_func.address(),
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
            kernel.delete_callback(cb->uid, id);
    }
    const std::lock_guard<std::mutex> lock(mutex);
    is_processing_callbacks = false;
    return processed;
}

void ThreadState::add_notified_callback() {
    const std::lock_guard<std::mutex> lock(mutex);
    ++notified_callbacks;
    cv.notify_all();
}

void ThreadState::remove_notified_callback() {
    const std::lock_guard<std::mutex> lock(mutex);
    if (notified_callbacks > 0)
        --notified_callbacks;
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
    // Vita, where the waker ends the wait. A host wait ends with its host call and stays WAITING.
    const bool ready = status == ThreadStatus::waiting && !host_waiting && wait_over();
    const WaitTarget reported = ready ? WaitTarget{} : wait_target;
    info.status = ready ? SCE_KERNEL_THREAD_STATUS_READY : std::to_underlying(shown_status());
    info.waitType = reported.callbacks ? reported.type | SCE_KERNEL_WAITTYPE_CB_BIT : reported.type;
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
    tls_slot(TLS_CURRENT_PRIORITY) = new_priority;
    return std::exchange(priority, new_priority);
}

SceInt32 ThreadState::change_affinity_mask(SceInt32 new_affinity_mask) {
    const std::lock_guard<std::mutex> lock(mutex);
    tls_slot(TLS_CPU_AFFINITY_MASK) = new_affinity_mask;
    return std::exchange(affinity_mask, new_affinity_mask);
}

void ThreadState::set_suspended(bool suspended) {
    const std::lock_guard<std::mutex> lock(mutex);
    debugger_hold = suspended;
    // A step the thread didn't take doesn't outlive the debugger's next call
    step_requested = false;
    if (suspended && runs())
        cpu->request_stop();
    cv.notify_all();
}

bool ThreadState::step() {
    std::unique_lock<std::mutex> lock(mutex);
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
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !runs(); });
}

bool ThreadState::stopped_at_breakpoint() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return debugger_hold && parked;
}

void ThreadState::pause_changed() {
    const std::lock_guard<std::mutex> lock(mutex);
    if (kernel.is_threads_paused() && runs())
        cpu->request_stop();
    cv.notify_all();
}

int ThreadState::vm_suspend() {
    std::unique_lock<std::mutex> lock(mutex);
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
    const std::lock_guard<std::mutex> lock(mutex);
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

SceInt32 ThreadState::get_vm_context(CPUContext &context, uint32_t &tpidrurw, bool &in_kernel_call) {
    std::unique_lock<std::mutex> lock(mutex);
    // A thread whose wait timed out while suspended stops on its way back to its code
    cv.wait(lock, [&] { return parked || status != ThreadStatus::running || vm_suspend_count == 0 || exiting(); });
    if (vm_suspend_count == 0)
        return SCE_KERNEL_ERROR_NOT_SUSPENDED;
    context = cpu->save_context();
    tpidrurw = cpu->get_tpidrurw();
    // Inside a kernel call means waiting, or held on the way back from one
    in_kernel_call = status == ThreadStatus::waiting || (parked && stopped_after_kernel_call);
    return SCE_KERNEL_OK;
}
