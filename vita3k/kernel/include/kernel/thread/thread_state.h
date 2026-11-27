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
#include <expected>
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

// Whether a game may create a thread with this priority, absolute or relative to the default one
inline bool is_user_priority(int priority) {
    return (priority >= SCE_KERNEL_HIGHEST_PRIORITY_USER && priority <= SCE_KERNEL_LOWEST_PRIORITY_USER)
        || (priority >= SCE_KERNEL_HIGHEST_DEFAULT_PRIORITY && priority <= SCE_KERNEL_LOWEST_DEFAULT_PRIORITY);
}

// Values are what sceKernelGetThreadInfo reports
enum class ThreadStatus : SceUInt32 {
    running = SCE_KERNEL_THREAD_STATUS_RUNNING,
    ready = SCE_KERNEL_THREAD_STATUS_READY, // Its wait ended, and it goes on to RUNNING once it runs
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

// A thread's VFP save area, which the kernel saves into when a call switches the thread out, and
// which the ForVM calls read and write while the thread is in a call
struct VfpArea {
    // d0 to d31
    std::array<float, arm::EXT_REG_WORDS> regs;
    // The FPSCR the save took, the kernel's
    uint32_t fpscr;

    // Fills every d register and gives the kernel's FPSCR, as each start and callback entry does
    void init();
    // Takes d8 to d15 of context and the kernel's FPSCR, when a call switches the thread out
    void save(const CPUContext &context);
    // Gives context d8 to d15 back and fills the others but d2 and d3, when the call goes on
    void resume(CPUContext &context) const;
    // Gives context d8 to d15 back and leaves the others, after a hold right after a call
    void give_back(CPUContext &context) const;
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

    // Lifecycle
    // Starts a dormant thread at entry_point, and its START handlers run first
    int start(SceSize arglen, Ptr<void> argp);
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
    // Starts the dormant thread from another thread and returns its exit status once it ends, or
    // the start's error. Used for module start and stop.
    std::expected<uint32_t, int> start_and_wait(SceSize args, Ptr<void> argp);
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
    // The priority the guest set, which a held ceiling may raise the one it runs at above
    int get_base_priority() const;
    // Sets the base priority, which the thread then runs at, and the one its TLS shows, unless it is
    // below the ceiling of a mutex it holds, which applies it at its free. Returns the priority it ran
    // at before.
    int change_base_priority(int new_priority);
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
    // exit_status. The end of each wait writes the time left to timeout.
    [[nodiscard]] SceInt32 wait_for_thread_end(ThreadState &waiter, SceInt32 *exit_status, SceUInt32 *timeout, bool callbacks);
    // Blocks this thread on nothing, showing target, until whoever ends the wait does, and returns its
    // result. A callback already pending ends a wait with callbacks at once with NOTIFY_CALLBACK, and
    // the caller then runs it with run_callbacks().
    [[nodiscard]] SceInt32 wait(WaitTarget target, Deadline deadline);
    // Runs the notified callbacks for a wait that ended with NOTIFY_CALLBACK, with no lock held.
    // Returns THREAD_STOPPED if the thread exits or is deleted meanwhile.
    [[nodiscard]] SceInt32 run_callbacks();
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
    // Notifies cb with arg from notifier_id and ends a wait with callbacks, unless cb is closed
    void notify_callback(Callback &cb, SceInt32 arg, SceUID notifier_id);
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
    // remove_held() for an end of this thread's own wait, with thread_mutex held
    void unlist_held(EventFlag &flag);
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
    friend class Event;
    friend class Mutex;

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
    // Shows the thread WAITING on the target, which switches it out in its call, so its VFP state goes
    // to the save area first. The lock must be held.
    void enter_wait(WaitTarget target);
    // Clears the target and shows the thread READY, as whoever ends the wait does. The lock must be held.
    void make_ready();
    // Gives the thread its VFP state back as it runs again in its call, and shows it RUNNING. The lock
    // must be held.
    void switch_in();
    // The status the guest sees: SUSPENDED for a thread a ForVM suspend holds at its stop
    ThreadStatus shown_status() const;
    // Whether the thread is ending
    bool exiting() const { return ending != Ending::none; }
    // Sets how the thread ends. Unless it's none, that stops its guest code for good and drops a step
    // it didn't take
    void set_ending(Ending how);
    // Whether something holds the thread before its guest code: the debugger, the emulator pause or
    // a ForVM suspend
    bool held() const;
    // Waits with thread_mutex held through lock until a ForVM suspend has stopped the thread.
    // Returns whether one still holds it.
    bool wait_vm_stop(std::unique_lock<std::mutex> &lock);
    // Whether the thread stopped inside a kernel call: waiting, READY after its wait, or held on the
    // way back from one
    bool stopped_in_kernel_call() const { return status == ThreadStatus::waiting || status == ThreadStatus::ready || (parked && stopped_after_kernel_call); }
    // Waits with thread_mutex held through lock until nothing holds the thread, parked meanwhile
    void stop_for_holds(std::unique_lock<std::mutex> &lock);
    // Whether the thread may run guest code now, or kernel code that changes its registers
    bool runs() const { return status == ThreadStatus::running && !parked && !host_waiting; }
    // Asks the thread to end, keeping a stronger request already made
    void request_ending(Ending how) { set_ending(std::max(ending, how)); }
    // Sets the priority and its copy in TLS, with thread_mutex held.
    void set_priority(int new_priority);
    // Moves the thread to its place for its priority in the queue it waits on, if any, with
    // thread_mutex held through lock and no object's lock held. Returns with lock dropped.
    void take_priority_place(std::unique_lock<std::mutex> &lock);
    // take_priority_place() with no lock held
    void take_priority_place();
    // Lists mutex as held, a ceiling mutex by its ceiling, and raises the thread to the ceiling if it
    // runs below it. Called with the mutex's lock held, for a thread that runs or is about to.
    void hold_mutex(Mutex &mutex);
    // Unlists mutex, recomputes the ceiling bound from the mutexes still held and runs the thread at
    // its base priority, or at the bound if that is higher. Returns whether the priority changed, so
    // that a waiting thread must move in its queue with take_priority_place(). Called with the
    // mutex's lock held.
    [[nodiscard]] bool free_mutex(Mutex &mutex);
    // Runs step on the object the thread is queued on, or on null when it is queued on none, with
    // thread_mutex held through lock and no object's lock held. Returns with lock dropped, and whether
    // the step ran, which it doesn't when the wait moved to another object meanwhile. A caller that
    // changes the thread first can ignore that, since a wait begun while no lock is held sees the
    // change at its start. The step checks again what it changes, since the same object may be
    // waited on again.
    template <typename Step>
    bool change_wait(std::unique_lock<std::mutex> &lock, Step step);
    // Sleeps WAITING until whoever ends the wait does, or ends it itself at the deadline with 0 for a
    // delay and WAIT_TIMEOUT for any other wait. Returns the wait's result, with thread_mutex held
    // through lock.
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

    // Lifecycle. RUNNING, READY, WAITING, DORMANT or DELETED, SUSPENDED is only reported.
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
    // The priority the guest set, which a start resets to init_priority
    int base_priority;
    // The highest ceiling of the mutexes it holds, SCE_KERNEL_LOWEST_PRIORITY_KERNEL for none
    int ceiling_bound = SCE_KERNEL_LOWEST_PRIORITY_KERNEL;
    // The priority the thread runs at, which its TLS and info show
    int priority;
    SceInt32 affinity_mask;

    // Waits
    // What the thread waits on, empty otherwise. The wait sets it and whoever ends the wait clears it.
    WaitTarget wait_target;
    // The object whose queue the thread is linked in, set and cleared with the link. Borrowed: the
    // thread links only inside its own wait call on the object, which holds it until the link is gone.
    Waitable *waited = nullptr;
    // The result of the current or last wait, set by whoever ends it
    SceInt32 wait_result = SCE_KERNEL_OK;
    // Where whoever ends the current wait writes the time left before wait_deadline, or null
    SceUInt32 *time_left = nullptr;
    Deadline wait_deadline;
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
    // mutexes, the ceiling ones first by their ceiling, and its write locks. Borrowed, each object's UID holds it, and a delete unlists it
    // before the UID goes.
    IntrusiveList<EventFlag, HeldLink<EventFlag>> held_flags;
    IntrusiveList<Mutex, HeldLink<Mutex>> held_mutexes;
    IntrusiveList<RWLock, HeldLink<RWLock>> held_rwlocks;

    // ForVM suspend
    // The sceKernelSuspendThreadForVM calls no sceKernelResumeThreadForVM undid yet
    int vm_suspend_count = 0;
    // The VFP save area, guarded by the lock
    VfpArea vfp_area;
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

std::size_t Waitable::wake_all_if(std::predicate<ThreadState &> auto pick, SceInt32 result) {
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

template <typename Step>
bool Waitable::change_wait(ThreadState &thread, std::unique_lock<std::mutex> &thread_lock, Step step) {
    thread_lock.unlock();
    // A delete that let go of the lock in its notify walk still has its waiters queued
    const std::unique_lock<std::mutex> guard(mutex);
    thread_lock.lock();
    // The wait may have ended, or moved to another object, while no lock was held
    const bool waits_here = thread.waited == this;
    if (waits_here)
        step(this);
    // Serving wakes other threads, so the thread's lock goes first
    thread_lock.unlock();
    // A deleted object has nothing to give
    if (!deleted)
        serve();
    return waits_here;
}

template <typename Step>
bool ThreadState::change_wait(std::unique_lock<std::mutex> &lock, Step step) {
    // A delay or no wait has no object
    if (!waited) {
        step(nullptr);
        lock.unlock();
        return true;
    }
    // Taken with the lock held, while the waiter's call still holds the object
    const Ref<Waitable> object = Ref<Waitable>::from(*waited);
    return object->change_wait(*this, lock, step);
}
