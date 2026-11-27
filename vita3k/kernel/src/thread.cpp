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

#include <cpu/arm.h>
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
#include <span>
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
    std::fill(ctx.cpu_registers.begin() + 2, ctx.cpu_registers.begin() + arm::REG_SP, 0xDEADBEEF);
    ctx.set_sp(stack_top);
    ctx.cpsr = SCE_KERNEL_THREAD_INITIAL_CPSR;
    ctx.fpscr = SCE_KERNEL_THREAD_INITIAL_FPSCR;
    return ctx;
}

// Puts the fill value the kernel uses for the d registers it doesn't keep in d register d
static void fill_d(std::span<float, arm::EXT_REG_WORDS> regs, uint32_t d) {
    regs[arm::WORDS_PER_D_REG * d] = std::bit_cast<float>(SCE_KERNEL_VFP_REGISTER_FILL_LOW);
    regs[arm::WORDS_PER_D_REG * d + 1] = std::bit_cast<float>(SCE_KERNEL_VFP_REGISTER_FILL_HIGH);
}

// The d8 to d15 part of a set of VFP registers, which a call keeps
template <class T>
static std::span<T, arm::AAPCS_VFP_CALLEE_SAVED_S_COUNT> callee_saved(std::span<T, arm::EXT_REG_WORDS> regs) {
    return regs.template subspan<arm::AAPCS_VFP_CALLEE_SAVED_FIRST_S, arm::AAPCS_VFP_CALLEE_SAVED_S_COUNT>();
}

// Puts the fill value in the d registers the kernel doesn't keep when it resumes a thread that it
// switched out in a kernel call
static void fill_unkept_vfp(CPUContext &ctx) {
    for (uint32_t d = 0; d < arm::D_REG_COUNT; d++) {
        if (d == 2 || d == 3 || (d >= arm::AAPCS_VFP_CALLEE_SAVED_FIRST_D && d <= arm::AAPCS_VFP_CALLEE_SAVED_LAST_D))
            continue;
        fill_d(ctx.fpu_registers, d);
    }
}

void VfpArea::init() {
    for (uint32_t d = 0; d < arm::D_REG_COUNT; d++)
        fill_d(regs, d);
    fpscr = SCE_KERNEL_THREAD_INITIAL_FPSCR;
}

void VfpArea::save(const CPUContext &context) {
    std::ranges::copy(callee_saved(std::span(context.fpu_registers)), callee_saved(std::span(regs)).begin());
    // The FPSCR the kernel runs with
    fpscr = SCE_KERNEL_THREAD_INITIAL_FPSCR;
}

void VfpArea::resume(CPUContext &context) const {
    give_back(context);
    fill_unkept_vfp(context);
}

void VfpArea::give_back(CPUContext &context) const {
    std::ranges::copy(callee_saved(std::span(regs)), callee_saved(std::span(context.fpu_registers)).begin());
}

void ThreadState::enter_wait(WaitTarget target) {
    wait_target = target;
    vfp_area.save(cpu->get_context());
    update_status(ThreadStatus::waiting);
}

void ThreadState::make_ready() {
    wait_target = {};
    update_status(ThreadStatus::ready);
}

void ThreadState::switch_in() {
    // The way back to the thread's code, where it keeps its own FPSCR
    CPUContext context = cpu->get_context();
    vfp_area.resume(context);
    cpu->set_context(context);
    update_status(ThreadStatus::running);
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
    , base_priority(this->init_priority)
    , priority(this->init_priority)
    , affinity_mask(affinity_mask) {
    uid = id;
    memset(stack.get_ptr<void>().get(mem), 0xcc, this->stack_size);
    memset(tls.get_ptr<void>().get(mem), 0, SCE_KERNEL_TLS_SIZE + kernel.tls_msize);

    tls_slot(TLS_THREAD_ID) = id;
    tls_slot(TLS_SP_TOP) = stack.get() + this->stack_size;
    tls_slot(TLS_SP_BOTTOM) = stack.get();
    // The console gives each thread a word from its MT19937. One fixed word keeps runs repeatable.
    tls_slot(TLS_JMPBUF_KEY) = static_cast<int>(0xFEFA7314);
    tls_slot(TLS_STACK_CHECK_GUARD) = static_cast<int>(KernelState::STACK_CHECK_GUARD);
    tls_slot(TLS_CURRENT_PRIORITY) = priority;
    tls_slot(TLS_CPU_AFFINITY_MASK) = affinity_mask;
    vfp_area.init();

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

int ThreadState::start(SceSize arglen, const Ptr<void> argp) {
    const std::lock_guard<std::mutex> thread_lock(thread_mutex);
    if (running_event_handlers)
        return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
    if (status != ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    if (arglen > stack_size)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    // A thread starts with TPIDRURW at 0, also when it starts again
    CPUContext ctx = initial_context(stack.get() + stack_size);
    // The entry gets the size of the argument data and its copy on the stack, or 0 and null for none
    Address data_addr = 0;
    if (argp && arglen > 0) {
        data_addr = ctx.get_sp() - align(arglen, 16);
        ctx.set_sp(data_addr);
        memcpy(Ptr<uint8_t>(data_addr).get(mem), argp.get(mem), arglen);
    } else {
        arglen = 0;
    }
    begin_run(ctx, { arglen, data_addr }, true);
    return SCE_KERNEL_OK;
}

void ThreadState::begin_run(CPUContext ctx, std::initializer_list<uint32_t> args, bool run_entry_callback) {
    assert(held_flags.empty() && held_mutexes.empty() && held_rwlocks.empty());
    run_start_callback = run_entry_callback;
    returned_value = SCE_KERNEL_ERROR_NOT_DORMANT;
    // Vita3K runs every thread as CPU 0, and a later start keeps the CPU fields
    if (last_cpu == -1) {
        last_cpu = 0;
        change_cpu_count = 0;
    }
    // A start clears a callback notification and a signal left from before, which its end keeps
    callback_pending = false;
    signal_pending = false;
    vfp_area.init();
    // Each run starts at the priority and on the cores the thread was created with, and its TLS
    // shows them
    base_priority = init_priority;
    ceiling_bound = SCE_KERNEL_LOWEST_PRIORITY_KERNEL;
    priority = init_priority;
    affinity_mask = init_affinity_mask;
    tls_slot(TLS_CURRENT_PRIORITY) = priority;
    tls_slot(TLS_CPU_AFFINITY_MASK) = affinity_mask;
    ctx.place_call(mem, entry_point, cpu->return_pc, args);
    cpu->set_context(ctx);
    update_status(ThreadStatus::running);
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
    std::unique_lock<std::mutex> lock(thread_mutex);
    request_ending(Ending::remove);
    // A dormant thread is gone at once
    if (status == ThreadStatus::dormant)
        update_status(ThreadStatus::deleted);
    else if (status == ThreadStatus::running)
        cpu->request_stop();
    // A kernel wait ends here with THREAD_STOPPED, a host wait with its host call
    else if (status == ThreadStatus::waiting && !host_waiting) {
        change_wait(lock, [&](Waitable *queue) {
            // The wait may end while the lock is let go, and a host wait begun then ends with its
            // host call
            if (status == ThreadStatus::waiting && !host_waiting)
                Waitable::wake_one(*this, queue, SCE_KERNEL_ERROR_THREAD_STOPPED);
        });
        return;
    }

    // Wake the thread if it is parked dormant, held before its guest code or in a host wait
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
            // The return lands in libkernel's exit stub, which pushes two words before its exit call
            cpu->stack_alloc(8);
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
        while (!held_flags.empty()) {
            const Ref<EventFlag> flag = Ref<EventFlag>::from(held_flags.front());
            lock.unlock();
            flag->release_if_held_by(*this);
            lock.lock();
        }
        while (!held_mutexes.empty()) {
            const Ref<Mutex> mutex = Ref<Mutex>::from(held_mutexes.front());
            lock.unlock();
            mutex->release_if_owned_by(*this);
            lock.lock();
        }
        // With no mutex left, the thread is back at its base priority
        ceiling_bound = SCE_KERNEL_LOWEST_PRIORITY_KERNEL;
        if (priority != base_priority)
            set_priority(base_priority);
        while (!held_rwlocks.empty()) {
            const Ref<RWLock> rwlock = Ref<RWLock>::from(held_rwlocks.front());
            lock.unlock();
            rwlock->release_if_write_locked_by(*this);
            lock.lock();
        }
        // The callbacks the thread created end with it, with their pending notifications
        while (!callbacks.empty())
            close_callback(callbacks.front());
        lock.unlock();
        // The end waiters see the status and the exit status together. Both locks stay held until
        // they are woken, so a start can't come in between.
        const std::lock_guard<std::mutex> end_guard(mutex);
        lock.lock();
        update_status(ending == Ending::exit ? ThreadStatus::dormant : ThreadStatus::deleted);
        wake_all_if([&](auto &waiter) {
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
    ctx.set_pc(caller.get_pc() | (caller.thumb() ? arm::THUMB_BIT : 0));
    ctx.cpsr = caller.cpsr;
    cpu->set_context(ctx);
    return returned;
}

std::optional<uint32_t> ThreadState::run_callback(Address fn, std::initializer_list<uint32_t> args) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    CPUContext saved = cpu->get_context();
    const uint32_t tpidruro = cpu->get_tpidruro();
    CPUContext entry = initial_context(saved.get_sp());
    vfp_area.init();
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
    const uint32_t sp_aligned = align_down(sp_orig - ctx_size, arm::AAPCS_STACK_ALIGN);

    auto *ctx = Ptr<uint32_t>(sp_aligned).get(cpu.mem);
    for (uint32_t i = 0; i < arm::REG_SP; i++)
        ctx[i] = cpu.get_reg(i);
    ctx[arm::REG_SP] = sp_aligned + ctx_size;
    ctx[arm::REG_LR] = cpu.get_lr();
    ctx[arm::REG_PC] = cpu.get_pc();
    ctx[16] = fault_addr;

    LOG_DEBUG("DABT handler=0x{:08X} FAR=0x{:08X} PC=0x{:08X} SP=0x{:08X} sp_aligned=0x{:08X}",
        handler, fault_addr, ctx[arm::REG_PC], sp_orig, sp_aligned);

    // The handler interrupts guest code, which goes on with all its registers
    const CPUContext interrupted = cpu.get_context();
    call_guest(handler, { sp_aligned });
    cpu.set_context(interrupted);
}

std::expected<uint32_t, int> ThreadState::start_and_wait(SceSize args, const Ptr<void> argp) {
    // A refused start leaves the thread dormant
    if (const int error = start(args, argp))
        return std::unexpected(error);
    std::unique_lock<std::mutex> lock(thread_mutex);
    cv.wait(lock, [&] { return status == ThreadStatus::dormant || status == ThreadStatus::deleted; });
    return returned_value;
}

uint32_t ThreadState::call_and_wait(Address arg) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    if (status != ThreadStatus::dormant)
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    begin_run(initial_context(stack.get() + stack_size), { arg }, false);
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
        // The deadline ends the delay with 0, and a notify runs the callbacks and waits again
        r = wait({ .type = wait_type(SCE_KERNEL_WAITTYPE_DELAY, callbacks) }, deadline);
        if (r != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return r;
        r = run_callbacks();
    }
    return r;
}

SceInt32 ThreadState::wait_for_signal(SceUInt32 *timeout, bool callbacks) {
    const Deadline deadline = deadline_from(timeout);
    SceInt32 r = callbacks ? run_callbacks() : SCE_KERNEL_OK;
    while (r == SCE_KERNEL_OK) {
        {
            std::unique_lock<std::mutex> lock(thread_mutex);
            // A signal sent before ends the wait at once, with the timeout as it was
            if (std::exchange(signal_pending, false))
                return SCE_KERNEL_OK;
            if (exiting())
                return SCE_KERNEL_ERROR_THREAD_STOPPED;
            // A signal wait sleeps with a callback pending, only a notify during the sleep ends it
            time_left = deadline == Deadline::max() ? nullptr : timeout;
            wait_deadline = deadline;
            enter_wait({ wait_type(SCE_KERNEL_WAITTYPE_SIGNAL, callbacks), SCE_KERNEL_SIGNAL_WAIT_ID });
            r = sleep(lock, deadline);
        }
        // A signal sent during the sleep ends it with 0, a notify runs the callbacks and waits again
        if (r != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return r;
        r = run_callbacks();
    }
    return r;
}

SceInt32 ThreadState::send_signal() {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    // Only a signal wait ends, and the signal it takes isn't kept. Any other wait goes on.
    if (status == ThreadStatus::waiting && wait_target.id == SCE_KERNEL_SIGNAL_WAIT_ID) {
        Waitable::wake_one(*this, nullptr, SCE_KERNEL_OK);
        return SCE_KERNEL_OK;
    }
    if (signal_pending)
        return SCE_KERNEL_ERROR_ALREADY_SENT;
    signal_pending = true;
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
    // Held right after a call, the thread is switched out in it
    const bool in_call = stopped_after_kernel_call;
    if (in_call)
        vfp_area.save(cpu->get_context());
    parked = true;
    cv.notify_all();
    // The holds can change while the thread waits
    cv.wait(lock, [&] { return !held(); });
    parked = false;
    // Without the fill, since an import's float result is already in d0
    if (in_call) {
        CPUContext context = cpu->get_context();
        vfp_area.give_back(context);
        cpu->set_context(context);
    }
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
    if (shown)
        thread.enter_wait(*shown);
    else
        thread.cv.notify_all();
}

ThreadState::HostWait::~HostWait() {
    const std::lock_guard<std::mutex> lock(thread.thread_mutex);
    thread.host_waiting = false;
    if (thread.status == ThreadStatus::waiting) {
        thread.make_ready();
        thread.switch_in();
    }
}

SceInt32 ThreadState::wait_for_thread_end(ThreadState &waiter, SceInt32 *exit_status, SceUInt32 *timeout, bool callbacks) {
    std::unique_lock<std::mutex> guard(mutex);
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
        const SceInt32 result = Waitable::wait(guard, waiter, type, { .out_status = exit_status }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return result;
    }
}

SceInt32 ThreadState::wait(WaitTarget target, Deadline deadline) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    if (exiting())
        return SCE_KERNEL_ERROR_THREAD_STOPPED;
    // A deadline already gone comes before a pending callback, so the wait still sleeps and its
    // timeout ends it at once
    if ((target.type & SCE_KERNEL_WAITTYPE_CB_BIT) && callback_pending && std::chrono::steady_clock::now() < deadline)
        return SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
    enter_wait(target);
    return sleep(lock, deadline);
}

SceInt32 ThreadState::sleep(std::unique_lock<std::mutex> &lock, Deadline deadline) {
    const auto ended = [&] { return status != ThreadStatus::waiting; };
    if (deadline == Deadline::max()) {
        cv.wait(lock, ended);
    } else if (!cv.wait_until(lock, deadline, ended)) {
        // A delay ends with 0, any other wait with WAIT_TIMEOUT, unless another ender came first
        // while the lock was dropped
        const bool delay = (wait_target.type & ~SCE_KERNEL_WAITTYPE_CB_BIT) == SCE_KERNEL_WAITTYPE_DELAY;
        const SceInt32 result = delay ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_WAIT_TIMEOUT;
        change_wait(lock, [&](Waitable *queue) {
            if (status == ThreadStatus::waiting)
                Waitable::wake_one(*this, queue, result);
        });
        lock.lock();
    }
    // Before the call writes its result to the registers. A thread something holds goes on to its
    // holds, which keep it from its callbacks and its code.
    switch_in();
    return wait_result;
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

void ThreadState::notify_callback(Callback &cb, SceInt32 arg, SceUID notifier_id) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    if (!cb.owner_link.linked())
        return;
    cb.note(arg, notifier_id);
    // Every notify reaches the owner, also one of a deleted callback that still runs
    callback_pending = true;
    // Read again in the step, since the wait may end while the lock is let go, and a new wait with
    // callbacks begins only once the callbacks ran
    const auto waits_for_callback = [&] { return status == ThreadStatus::waiting && (wait_target.type & SCE_KERNEL_WAITTYPE_CB_BIT) && callback_pending; };
    // A thread that doesn't wait with callbacks only keeps the notification
    if (waits_for_callback())
        change_wait(lock, [&](Waitable *queue) {
            if (waits_for_callback())
                Waitable::wake_one(*this, queue, SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
        });
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
    callbacks.push_back(*cb);
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
    // Unlisted first, since removing the UID can free cb
    callbacks.remove(cb);
    kernel.objects.remove<Callback>(cb.uid, id);
}

void ThreadState::add_held(EventFlag &flag) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    held_flags.push_back(flag);
}

void ThreadState::remove_held(EventFlag &flag) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    unlist_held(flag);
}

void ThreadState::unlist_held(EventFlag &flag) {
    assert(flag.held_link.linked());
    held_flags.remove(flag);
}

void ThreadState::hold_mutex(Mutex &mutex) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    // A plain mutex goes at the tail and changes nothing else
    if (!(mutex.attr & SCE_KERNEL_MUTEX_ATTR_CEILING)) {
        held_mutexes.push_back(mutex);
        return;
    }
    // Behind the held ceilings at or above this one, and ahead of every plain mutex, whose ceiling is
    // the lowest priority
    auto pos = held_mutexes.begin();
    while (pos != held_mutexes.end() && pos->ceiling <= mutex.ceiling)
        ++pos;
    held_mutexes.insert(pos, mutex);
    ceiling_bound = std::min(ceiling_bound, mutex.ceiling);
    // A thread that runs at or above the ceiling keeps its priority
    if (priority > ceiling_bound)
        set_priority(ceiling_bound);
}

bool ThreadState::free_mutex(Mutex &mutex) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    assert(mutex.held_link.linked());
    held_mutexes.remove(mutex);
    // The first held mutex has the highest ceiling, if it has one at all
    const bool ceiling = !held_mutexes.empty() && (held_mutexes.front().attr & SCE_KERNEL_MUTEX_ATTR_CEILING);
    ceiling_bound = ceiling ? held_mutexes.front().ceiling : SCE_KERNEL_LOWEST_PRIORITY_KERNEL;
    const int bounded = std::min(base_priority, ceiling_bound);
    if (bounded == priority)
        return false;
    set_priority(bounded);
    return true;
}
void ThreadState::add_held(RWLock &rwlock) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    held_rwlocks.push_back(rwlock);
}

void ThreadState::remove_held(RWLock &rwlock) {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    assert(rwlock.held_link.linked());
    held_rwlocks.remove(rwlock);
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
        // The run keeps the handler listed, and its reference keeps it alive once an unregister
        // removes the UID
        ++handler.runs;
        const ThreadEventHandlerRef held = ThreadEventHandlerRef::from(handler);
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
    // The target shows only while the thread is WAITING, since on a Vita a wait stores both at once
    const WaitTarget reported = status == ThreadStatus::waiting ? wait_target : WaitTarget{};
    info.status = std::to_underlying(shown_status());
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

int ThreadState::get_base_priority() const {
    const std::lock_guard<std::mutex> lock(thread_mutex);
    return base_priority;
}

int ThreadState::change_base_priority(int new_priority) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    // What the call returns
    const int old_priority = priority;
    base_priority = new_priority;
    // A base below the ceiling of a held mutex waits for that mutex's free
    if (new_priority > ceiling_bound)
        return old_priority;
    set_priority(new_priority);
    take_priority_place(lock);
    return old_priority;
}

void ThreadState::set_priority(int new_priority) {
    priority = new_priority;
    tls_slot(TLS_CURRENT_PRIORITY) = new_priority;
}

void ThreadState::take_priority_place() {
    std::unique_lock<std::mutex> lock(thread_mutex);
    take_priority_place(lock);
}

void ThreadState::take_priority_place(std::unique_lock<std::mutex> &lock) {
    // A wait that moved to another object meanwhile moves there. The priority is read in the step,
    // since a newer change may come in while no lock is held, and the last one wins.
    while (!change_wait(lock, [&](Waitable *queue) {
        if (queue) {
            queued_priority = priority;
            queue->requeue(*this);
        }
    }))
        lock.lock();
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
    step_requested = (status == ThreadStatus::running || status == ThreadStatus::ready || status == ThreadStatus::waiting) && !exiting();
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

bool ThreadState::wait_vm_stop(std::unique_lock<std::mutex> &lock) {
    // A thread whose wait timed out while suspended stops on its way back to its code
    cv.wait(lock, [&] { return parked || status != ThreadStatus::running || vm_suspend_count == 0; });
    return vm_suspend_count != 0;
}

SceInt32 ThreadState::get_vm_context(SceKernelThreadCpuRegisterInfo *cpu_info, SceKernelThreadVfpRegisterInfo *vfp_info) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    if (!wait_vm_stop(lock))
        return SCE_KERNEL_ERROR_NOT_SUSPENDED;
    const CPUContext context = cpu->get_context();
    if (cpu_info) {
        cpu_info->cpsr = context.cpsr;
        std::ranges::copy(context.cpu_registers, cpu_info->reg);
        cpu_info->tpidrurw = context.tpidrurw;
        cpu_info->sb = tls_slot(TLS_SP_TOP);
        cpu_info->st = tls_slot(TLS_SP_BOTTOM);
    }
    if (vfp_info) {
        // Inside a kernel call it is the save area
        if (stopped_in_kernel_call()) {
            vfp_info->fpscr = vfp_area.fpscr;
            std::ranges::copy(vfp_area.regs, vfp_info->reg);
        } else {
            vfp_info->fpscr = context.fpscr;
            std::ranges::copy(context.fpu_registers, vfp_info->reg);
        }
    }
    return SCE_KERNEL_OK;
}

SceInt32 ThreadState::set_vm_context(const SceKernelThreadCpuRegisterInfo *cpu_info, const SceKernelThreadVfpRegisterInfo *vfp_info) {
    std::unique_lock<std::mutex> lock(thread_mutex);
    if (!wait_vm_stop(lock))
        return SCE_KERNEL_ERROR_NOT_SUSPENDED;
    CPUContext context = cpu->get_context();
    if (cpu_info) {
        std::ranges::copy(cpu_info->reg, context.cpu_registers.begin());
        context.cpsr = (context.cpsr & SCE_KERNEL_VM_CONTEXT_CPSR_KEPT) | (cpu_info->cpsr & ~SCE_KERNEL_VM_CONTEXT_CPSR_KEPT);
        context.tpidrurw = cpu_info->tpidrurw;
        tls_slot(TLS_SP_TOP) = static_cast<int>(cpu_info->sb);
        tls_slot(TLS_SP_BOTTOM) = static_cast<int>(cpu_info->st);
    }
    if (vfp_info) {
        // The FPSCR goes to the thread, and inside a kernel call the d registers go to the save area
        context.fpscr = vfp_info->fpscr;
        if (stopped_in_kernel_call())
            std::ranges::copy(vfp_info->reg, vfp_area.regs.begin());
        else
            std::ranges::copy(vfp_info->reg, context.fpu_registers.begin());
    }
    cpu->set_context(context);
    return SCE_KERNEL_OK;
}
