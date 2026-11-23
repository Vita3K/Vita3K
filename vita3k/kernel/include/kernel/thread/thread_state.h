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
#include <kernel/callback.h>
#include <kernel/types.h>
#include <kernel/waitable.h>
#include <mem/block.h>
#include <mem/ptr.h>

#include <algorithm>
#include <condition_variable>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

struct ThreadState;
using ThreadStateRef = Ref<ThreadState>;
struct KernelState;
class Callback;
using CallbackRef = Ref<Callback>;
class EventFlag;
class Mutex;
class RWLock;

// Picks the held link of an event flag, mutex or rwlock, for the lists declared before those are
// complete
template <class T>
struct HeldLink : MemberLink<&T::held_link> {};

// Values are what sceKernelGetThreadInfo reports
enum class ThreadStatus : SceUInt32 {
    running = SCE_KERNEL_THREAD_STATUS_RUNNING,
    waiting = SCE_KERNEL_THREAD_STATUS_WAITING, // Waiting to be awaken by sync object or operation
    dormant = SCE_KERNEL_THREAD_STATUS_DORMANT, // Waiting for a job
    deleted = SCE_KERNEL_THREAD_STATUS_DELETED, // Gone for guest calls, until its host thread ends
    suspended = SCE_KERNEL_THREAD_STATUS_SUSPENDED, // Held by a ForVM suspend, only reported
};

// A thread, which its end waiters wait on like on any waitable object, and which is itself linked
// in the wait queue of what it waits on. Lock order: the object's lock, which guards the end
// waiters, then thread_mutex, then the thread_mutex of an end waiter it wakes.
// Where a thread's display waits count from: the vblank count its last display wait ended on, one
// for each depth of display waits inside callbacks
struct DisplayWaitBases {
    uint64_t bases[7] = {};
    // How many of the thread's display waits are going on
    uint32_t depth = 0;
};

struct ThreadState final : public WithUidClass<ThreadState, Waitable, UidClass::thread> {
private:
    friend class Ref<ThreadState>;

    // A dormant thread with its stack and TLS, which runs on cpu
    ThreadState(SceUID id, KernelState &kernel, MemState &mem, CPUStatePtr cpu, const char *name, Address entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr);

public:
    // The thread's place in the queue of the object it waits on
    IntrusiveLink<ThreadState> wait_link;

    // Fixed when the thread is created
    const SceUID id;
    const Address entry_point;
    // What thread info reports, the creation attributes and SCE_KERNEL_THREAD_ATTR_USER
    const SceUInt32 thread_attr;
    // The stack size, a whole number of pages
    const int stack_size;
    const Block stack;
    const Block tls;
    // The pointer is fixed. The registers are only touched by the thread itself, or while
    // something holds it.
    const CPUStatePtr cpu;
    // The priority and the cores it was created with, which each start goes back to
    const int init_priority;
    const SceInt32 init_affinity_mask;

    // A thread only goes away once its end gave back what it held
    ~ThreadState() override;

    // The thread's current wait, guarded by the lock of the object it waits on
    // What it asked for, which the wakers read. Only queueing the thread sets it.
    const WaitRequest &wait_request() const { return request; }
    // Its priority when it was queued or last requeued
    int queued_priority = 0;
    // The result given by whoever ended the queued wait, apart from the wake token wait_result
    std::optional<SceInt32> ender_result;

    // Lifecycle
    // Starts a dormant thread at entry_point.
    int start(SceSize arglen, Ptr<void> argp, bool run_entry_callback = false);
    // Ends the thread like sceKernelExitThread: the end handler runs, then it parks dormant
    void exit(SceInt32 status);
    // Ends the thread like sceKernelExitDeleteThread: the end handler runs, then it's deleted
    void exit_delete(SceInt32 status);
    // Deletes the thread like sceKernelDeleteThread, or returns NOT_DORMANT if it isn't dormant
    SceInt32 remove_dormant();
    // Deletes a dormant thread, or one being torn down, without running its end handler
    void remove();
    // Runs the thread on its host thread: parks it dormant between starts, runs its guest code and
    // ends it, until it's deleted
    void run_loop();
    // Calls a guest function on the thread's CPU from inside a kernel call, usually one the thread
    // itself made, like a function pointer call from library code. The registers are then the ones
    // the function leaves. Returns its r0, or nothing if the thread's or the app's end cut it short.
    std::optional<uint32_t> call_guest(Address fn, std::initializer_list<uint32_t> args);
    // Starts the dormant thread from another thread and returns its exit status once it ends, or the
    // start's error. Used for module start and stop.
    uint32_t start_and_wait(SceSize args, Ptr<void> argp);
    // Runs the dormant thread's entry as a call with one argument from another thread and returns
    // its exit status once it ends. Used for the GXM display queue callback.
    uint32_t call_and_wait(Address arg);

    // Status and info
    ThreadStatus get_status() const;
    // The thread's info, as sceKernelGetThreadInfo reports it
    SceKernelThreadInfo info();
    // The room left on the stack, all of it for a dormant thread
    int stack_free_size() const;
    // A word of the thread's TLS, by its TLS_* index
    int &tls_slot(int index) const { return tls.get_ptr<int>().get(mem)[index]; }

    // Priority and affinity
    // The priority the thread runs at
    int get_priority() const;
    // Changes the priority the thread runs at, and the one its TLS shows. Returns the old priority.
    int change_priority(int new_priority);
    // Changes the cores the thread may run on, and the ones its TLS shows. Returns the old mask.
    SceInt32 change_affinity_mask(SceInt32 new_affinity_mask);

    // Waits
    // Blocks this thread until the deadline passes.
    [[nodiscard]] SceInt32 delay_until(Deadline deadline, bool callbacks);
    // Blocks this thread until a signal is sent to it or the timeout passes, writing the time left back.
    [[nodiscard]] SceInt32 wait_for_signal(SceUInt32 *timeout, bool callbacks);
    // Sends a signal to this thread, which ends its signal wait or else stays pending. Fails if the
    // previous one is still pending.
    SceInt32 send_signal();
    // Blocks waiter until this thread becomes dormant or deleted, then writes its exit status to
    // exit_status. The time left is written back to timeout after every sleep.
    [[nodiscard]] SceInt32 wait_for_thread_end(ThreadState &waiter, SceInt32 *exit_status, SceUInt32 *timeout, bool callbacks);
    // sleep(), but a callback already pending ends a wait with callbacks at once with NOTIFY_CALLBACK,
    // and the caller then runs it with run_callbacks(). A stale wake can end it early, so callers must
    // recheck their condition. For a wait that queues on nothing, which shows target while it lasts.
    [[nodiscard]] SceInt32 wait(WaitTarget target, Deadline deadline);
    // wait() for a thread a queue holds, which shows the target the queue set.
    [[nodiscard]] SceInt32 wait(Deadline deadline);
    // Runs the notified callbacks for a wait that ended with NOTIFY_CALLBACK, with no lock held.
    // Returns THREAD_STOPPED if the thread exits or is deleted meanwhile.
    [[nodiscard]] SceInt32 run_callbacks();
    // Ends this thread's queued wait, so it shows no target and wait() returns.
    void wake();
    // A host call that blocks the thread inside an export, like an audio output, during which the
    // thread counts as stopped. It shows WAITING on shown while it lives, or stays RUNNING without
    // a target.
    class HostWait {
    public:
        explicit HostWait(ThreadState &thread, std::optional<WaitTarget> shown = WaitTarget{});
        ~HostWait();
        HostWait(const HostWait &) = delete;
        HostWait &operator=(const HostWait &) = delete;

    private:
        ThreadState &thread;
    };
    // Only the thread itself touches it
    DisplayWaitBases display_waits;

    // Callbacks this thread created
    // Lists cb, which this thread creates, under a new UID and returns the UID
    SceUID add_callback(CallbackRef cb);
    // Notifies cb with arg from notifier_id and wakes a wait with callbacks, or returns false if cb
    // is closed
    bool notify_callback(Callback &cb, SceInt32 arg, SceUID notifier_id);
    // Drops cb's pending notifications
    void cancel_callback(Callback &cb);
    // cb's pending notifications
    Callback::Notification callback_notifications(const Callback &cb);
    // Deletes cb, or returns false if it was deleted already. A run of it going on keeps its UID
    // until the run ends.
    bool delete_callback(Callback &cb);
    // Runs the notified callbacks of this thread and returns how many ran. Called by the thread itself.
    SceUInt32 process_callbacks();
    // Returns whether a callback of this thread was notified since its last callback pass
    bool has_pending_callbacks();

    // What the thread holds, which its end gives back. Called with the object's lock held.
    void add_held(EventFlag &flag);
    void remove_held(EventFlag &flag);
    void add_held(Mutex &mutex);
    void remove_held(Mutex &mutex);
    void add_held(RWLock &rwlock);
    void remove_held(RWLock &rwlock);

    // Thread event handlers
    // Whether the thread runs its thread event handlers.
    bool in_event_handler() const;

    // Holds: the debugger, the emulator pause and the ForVM suspend
    // Stops the thread for the debugger the next time it would run guest code, or lets it run again.
    // A pause or a ForVM suspend may still hold it after the debugger lets go.
    void set_suspended(bool suspended);
    // Lets a thread the debugger stopped run one instruction, then waits until it stops again or is
    // dormant or deleted. Returns false and doesn't step while something else holds the thread.
    bool step();
    // Waits until the thread doesn't run guest code
    void wait_until_not_running();
    // Whether the debugger holds the thread and it has stopped. Once the debugger has let every
    // thread go, only a breakpoint or a finished step holds a thread for it again.
    bool stopped_at_breakpoint() const;
    // Stops the thread's guest code for the emulator pause KernelState just set, or lets the thread
    // run again after it
    void pause_changed();

    // ForVM suspend
    // Suspends the thread for sceKernelSuspendThreadForVM: it doesn't run guest code until as many
    // vm_resume calls. vm_suspend returns once it has stopped. Each returns the suspend count after
    // it, or an error for a dormant thread or a resume with no suspend.
    int vm_suspend();
    int vm_resume();
    // Waits until a ForVM suspend has stopped the thread, then reads the registers of the blocks
    // given from the context it stopped with. Returns NOT_SUSPENDED if no ForVM suspend holds it.
    SceInt32 get_vm_context(SceKernelThreadCpuRegisterInfo *cpu_info, SceKernelThreadVfpRegisterInfo *vfp_info);
    // Waits until a ForVM suspend has stopped the thread, then writes the registers of the blocks
    // given into the context it stopped with. Returns NOT_SUSPENDED if no ForVM suspend holds it.
    SceInt32 set_vm_context(const SceKernelThreadCpuRegisterInfo *cpu_info, const SceKernelThreadVfpRegisterInfo *vfp_info);

private:
    friend class Waitable;

    WaitRequest request;

    // Resets what each run starts with and runs the entry with these arguments, under the thread lock
    void begin_run(CPUContext ctx, std::initializer_list<uint32_t> args, bool run_entry_callback);

    // How a thread ends, each value stronger than the ones before it
    enum class Ending : uint8_t {
        none,
        // An exit, or a return from the entry: the end handler runs, then the thread parks dormant
        exit,
        // The end handler runs, then the host thread ends
        exit_delete,
        // The host thread ends without running the end handler
        remove,
    };

    // The private methods are called with thread_mutex held, unless they say otherwise
    void update_status(ThreadStatus status);
    // The status the guest sees: SUSPENDED for a thread a ForVM suspend holds at its stop
    ThreadStatus shown_status() const;
    // Whether the thread is ending
    bool exiting() const { return ending != Ending::none; }
    // Sets how the thread ends. Unless it's none, that stops its guest code for good, drops a step
    // it didn't take and clears its wait target
    void set_ending(Ending how);
    // Whether something holds the thread before its guest code: the debugger, the emulator pause or
    // a ForVM suspend
    bool held() const;
    // Waits with thread_mutex held through lock until a ForVM suspend has stopped the thread.
    // Returns whether one still holds it.
    bool wait_vm_stop(std::unique_lock<std::mutex> &lock);
    // Whether the thread stopped inside a kernel call: waiting, or held on the way back from one
    bool stopped_in_kernel_call() const { return status == ThreadStatus::waiting || (parked && stopped_after_kernel_call); }
    // Waits with thread_mutex held through lock until nothing holds the thread, parked meanwhile
    void stop_for_holds(std::unique_lock<std::mutex> &lock);
    // Whether the thread may run guest code now, or kernel code that changes its registers
    bool runs() const { return status == ThreadStatus::running && !parked && !host_waiting; }
    // Asks the thread to end, keeping a stronger request already made
    void request_ending(Ending how) { set_ending(std::max(ending, how)); }
    // Whether the thread's current wait has a result or the thread is ending. With thread_mutex held.
    bool wait_over() const { return exiting() || wait_result.has_value(); }
    // wake() with thread_mutex held.
    void end_wait();
    // Sets the priority and its copy in TLS, with thread_mutex held.
    void set_priority(int new_priority);
    // wait() with thread_mutex held through lock.
    [[nodiscard]] SceInt32 wait(std::unique_lock<std::mutex> &lock, Deadline deadline);
    // Sleeps until wake(), a callback notify during the sleep if wait_target has the callback bit, the
    // deadline, or the thread's exit or delete (THREAD_STOPPED), with thread_mutex held through lock.
    [[nodiscard]] SceInt32 sleep(std::unique_lock<std::mutex> &lock, Deadline deadline);

    // Runs guest code at one level, the thread's run or a call, with thread_mutex held through lock, which
    // it releases around guest code and the handling of each exit. A call the kernel makes starts
    // from_kernel_call. Returns r0 once the level's function returns, or nothing once the
    // thread is ending.
    std::optional<uint32_t> run_guest(std::unique_lock<std::mutex> &lock, bool from_kernel_call);
    // Runs fn with args from ctx until it returns, with thread_mutex held through lock. Returns r0, or
    // nothing once the thread is ending.
    std::optional<uint32_t> enter_guest(std::unique_lock<std::mutex> &lock, CPUContext ctx, Address fn, std::initializer_list<uint32_t> args);
    // Runs a callback or a thread event handler like the kernel does, from the registers of a new
    // thread on the caller's stack, and puts the caller's registers back after. Returns its r0, or
    // nothing if the thread's or the app's end cut it short.
    std::optional<uint32_t> run_callback(Address fn, std::initializer_list<uint32_t> args);
    // Ends the thread's run and returns whether it parks dormant for another start
    bool finish_run(std::unique_lock<std::mutex> &lock);
    // Calls the guest's data abort handler with the abort context on the stack, without thread_mutex held
    void dispatch_abort(Address fault_addr);
    // Unlists cb and removes its UID, with thread_mutex held
    void close_callback(Callback &cb);
    // Runs the thread's event handlers for type in registration order, releasing thread_mutex
    // through lock while they run
    void run_event_handlers(std::unique_lock<std::mutex> &lock, SceUInt32 type);
    // Exits the thread as how with status, or ends the app inside a callback or a thread event
    // handler. Without thread_mutex held.
    void request_exit(Ending how, SceInt32 status);

    // Fixed when the thread is created
    KernelState &kernel;
    MemState &mem;

    mutable std::mutex thread_mutex;
    // The one condition variable. The thread's waits, its dormant park and its holds wait on it,
    // and so do other threads that wait for its status. Every change any of them reads notifies all.
    std::condition_variable cv;

    // The rest is guarded by mutex, unless it says otherwise

    // Lifecycle. RUNNING, WAITING, DORMANT or DELETED, SUSPENDED is only reported.
    ThreadStatus status = ThreadStatus::dormant;
    // The exit status the guest sees: DORMANT until the first start, NOT_DORMANT while the thread
    // runs, then what it returned or exited with
    uint32_t returned_value = SCE_KERNEL_ERROR_DORMANT;
    // The CPU the thread last ran on and how often it changed CPU, -1 until its first run
    SceInt32 last_cpu = -1;
    SceInt32 change_cpu_count = -1;
    Ending ending = Ending::none;
    // Run the thread start event once before guest code. Set by start().
    bool run_start_callback = false;
    // The thread runs its thread event handlers. Set and cleared by run_event_handlers().
    bool running_event_handlers = false;

    // Priority and affinity
    int priority;
    SceInt32 affinity_mask;

    // Waits
    // What the thread waits on, empty otherwise. A queue sets it, and the waker or the wait's own end
    // clears it. A wait that queues on nothing sets and clears it itself.
    WaitTarget wait_target;
    // The result the next wait() returns, set by the first wake() or callback notify
    std::optional<SceInt32> wait_result;
    // A sceKernelSendSignal is pending for this thread.
    bool signal_pending = false;

    // Callbacks
    // A callback of this thread was notified since the start of the last callback pass
    bool callback_pending = false;
    // The callback runs going on, nested ones too
    int callback_depth = 0;
    // The thread's callbacks in creation order, including one whose delete is in progress. Borrowed,
    // each callback's UID holds it until close_callback unlists it.
    IntrusiveList<Callback, MemberLink<&Callback::owner_link>> callbacks;

    // What the thread holds, each list in the order it took them: its event flag holds, its heavy
    // mutexes and its write locks. Borrowed, each object's UID holds it, and a delete unlists it
    // before the UID goes.
    IntrusiveList<EventFlag, HeldLink<EventFlag>> held_flags;
    IntrusiveList<Mutex, HeldLink<Mutex>> held_mutexes;
    IntrusiveList<RWLock, HeldLink<RWLock>> held_rwlocks;

    // ForVM suspend
    // The sceKernelSuspendThreadForVM calls no sceKernelResumeThreadForVM undid yet
    int vm_suspend_count = 0;
    // Whether the guest code of the level about to run last stopped right after a kernel call, which
    // a hold there reports. run_guest() sets it before the holds.
    bool stopped_after_kernel_call = false;

    // The host's control, which the guest never sees
    // The debugger holds the thread, also after a breakpoint or a finished step
    bool debugger_hold = false;
    // The debugger asked for one instruction, which the next run of guest code takes
    bool step_requested = false;
    // The thread waits for its holds before guest code, with its registers still
    bool parked = false;
    // Blocked in a host wait inside an export
    bool host_waiting = false;
};

ThreadState *Waitable::find_waiter(std::predicate<ThreadState &> auto pick) {
    for (ThreadState &thread : waiters) {
        if (pick(thread))
            return &thread;
    }
    return nullptr;
}

std::size_t Waitable::wake_waiters_if(std::predicate<ThreadState &> auto pick, SceInt32 result) {
    std::size_t woken = 0;
    for (auto it = waiters.begin(); it != waiters.end();) {
        ThreadState &thread = *it++;
        if (!pick(thread))
            continue;
        wake_waiter(thread, result);
        ++woken;
    }
    return woken;
}
