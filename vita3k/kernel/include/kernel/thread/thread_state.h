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
#include <kernel/thread/wait_queue.h>
#include <kernel/types.h>
#include <mem/block.h>
#include <mem/ptr.h>

#include <algorithm>
#include <condition_variable>
#include <initializer_list>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

struct ThreadState;
using ThreadStatePtr = std::shared_ptr<ThreadState>;
struct KernelState;
class Callback;
using CallbackPtr = std::shared_ptr<Callback>;

// Values are what sceKernelGetThreadInfo reports
enum class ThreadStatus : SceUInt32 {
    running = SCE_KERNEL_THREAD_STATUS_RUNNING,
    waiting = SCE_KERNEL_THREAD_STATUS_WAITING, // Waiting to be awaken by sync object or operation
    dormant = SCE_KERNEL_THREAD_STATUS_DORMANT, // Waiting for a job
    deleted = SCE_KERNEL_THREAD_STATUS_DELETED, // Gone for guest calls, until its host thread ends
    suspended = SCE_KERNEL_THREAD_STATUS_SUSPENDED, // Held by a ForVM suspend, only reported
};

struct ThreadState final {
    // Fixed when the thread is created
    const SceUID id;
    const std::string name;
    const Address entry_point;
    // What thread info reports, the creation attributes and SCE_KERNEL_THREAD_ATTR_USER
    const SceUInt32 attr;
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

    // A dormant thread with its stack and TLS, which runs on cpu
    ThreadState(SceUID id, KernelState &kernel, MemState &mem, CPUStatePtr cpu, const char *name, Address entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr);

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
    // itself made, and returns its r0, or the exit status if the thread ends during it. With
    // fresh_vfp_state, the call starts with the FPSCR and VFP exception setting of a new thread.
    uint32_t call_guest(Address fn, std::initializer_list<uint32_t> args, bool fresh_vfp_state = false);
    // Starts the dormant thread from another thread and returns its exit status once it ends. Used
    // for module start and stop and the GXM display queue.
    uint32_t start_and_wait(SceSize args, const Ptr<void> argp = Ptr<void>{});

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
    // Sends a signal to this thread. Fails if the previous one was not consumed yet.
    SceInt32 send_signal();
    // Blocks waiter until this thread becomes dormant, then writes its exit status to exit_status.
    [[nodiscard]] SceInt32 wait_for_thread_end(const ThreadStatePtr &waiter, SceInt32 *exit_status, Deadline deadline, bool callbacks);
    // Waits on target until woken by wake(), the deadline passes, or the thread exits or is deleted,
    // which ends it with THREAD_STOPPED.
    // With target.callbacks, a notified callback ends it with NOTIFY_CALLBACK, and the caller runs
    // it with run_callbacks(). A stale wake can end it early, so callers must recheck their condition.
    [[nodiscard]] SceInt32 wait(WaitTarget target, Deadline deadline);
    // Runs the notified callbacks for a wait that ended with NOTIFY_CALLBACK, with no lock held.
    // Returns THREAD_STOPPED if the thread exits or is deleted meanwhile.
    [[nodiscard]] SceInt32 run_callbacks();
    // Wakes this thread from wait().
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
    // The vblank a display wait for vcount vblanks ends on: vcount after the one the thread's last
    // display wait ended on, and at least next_vblank. The next display wait counts from it.
    uint64_t next_display_wait(uint64_t next_vblank, int vcount);

    // Callbacks
    // Runs the notified callbacks of this thread and returns how many ran. Called by the thread itself.
    SceUInt32 process_callbacks();
    // Returns whether a notified callback would run now, which none does inside a callback. Takes mutex.
    bool has_pending_callbacks();
    // Adds a callback this thread created. Called by the thread itself.
    void add_callback(const CallbackPtr &cb) { callbacks.push_back(cb); }
    // Counts a callback of this thread that was notified and hasn't run for it yet, waking a wait
    // with callbacks, and stops counting it. Called by the callback with its lock held.
    void add_notified_callback();
    void remove_notified_callback();

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
    // Waits until a ForVM suspend has stopped the thread, then gets the context it stopped with, its
    // TPIDRURW and whether it stopped inside a kernel call. Returns NOT_SUSPENDED if no ForVM
    // suspend holds it.
    SceInt32 get_vm_context(CPUContext &context, uint32_t &tpidrurw, bool &in_kernel_call);

private:
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

    struct EndWaitEntry {
        // Where to write the exit status, or null
        SceInt32 *exit_status;
    };

    // The private methods are called with mutex held, unless they say otherwise
    void update_status(ThreadStatus status);
    // The status the guest sees: SUSPENDED for a thread a ForVM suspend holds at its stop
    ThreadStatus shown_status() const;
    // Whether the thread is ending
    bool exiting() const { return ending != Ending::none; }
    // Sets how the thread ends, which stops its guest code for good and drops a step it didn't take
    // unless it's none
    void set_ending(Ending how);
    // Whether something holds the thread before its guest code: the debugger, the emulator pause or
    // a ForVM suspend
    bool held() const;
    // Waits with mutex held through lock until nothing holds the thread, parked meanwhile
    void stop_for_holds(std::unique_lock<std::mutex> &lock);
    // Whether the thread may run guest code now, or kernel code that changes its registers
    bool runs() const { return status == ThreadStatus::running && !parked && !host_waiting; }
    // Asks the thread to end, keeping a stronger request already made
    void request_ending(Ending how) { set_ending(std::max(ending, how)); }
    // Whether a callback of this thread was notified and hasn't run yet
    bool callbacks_notified() const { return notified_callbacks > 0; }
    // Whether a notified callback ends the current wait. Callbacks don't nest, so a wait inside a
    // callback runs none. With mutex held.
    bool callback_ends_wait() const { return wait_target.callbacks && !is_processing_callbacks && callbacks_notified(); }
    // Whether the thread's current wait is over, by a wake, its end or a callback it runs, though
    // the thread hasn't gone on yet. With mutex held.
    bool wait_over() const { return exiting() || wake_pending || callback_ends_wait(); }

    // Runs guest code at one level, the thread's run or a call, with mutex held through lock, which
    // it releases around guest code and the handling of each exit. A call the kernel makes starts
    // from_kernel_call. Returns r0 once the level's function returns, or nothing once the
    // thread is ending.
    std::optional<uint32_t> run_guest(std::unique_lock<std::mutex> &lock, bool from_kernel_call);
    // Ends the thread's run and returns whether it parks dormant for another start
    bool finish_run(std::unique_lock<std::mutex> &lock);
    // Calls the guest's data abort handler with the abort context on the stack, without mutex held
    void dispatch_abort(Address fault_addr);

    // Fixed when the thread is created
    KernelState &kernel;
    MemState &mem;

    mutable std::mutex mutex;
    // The one condition variable. The thread's waits, its dormant park and its holds wait on it,
    // and so do other threads that wait for its status. Every change any of them reads notifies all.
    std::condition_variable cv;

    // The rest is guarded by mutex, unless it says otherwise

    // Lifecycle. RUNNING, WAITING, DORMANT or DELETED, SUSPENDED is only reported.
    ThreadStatus status = ThreadStatus::dormant;
    // The exit status the guest sees: DORMANT until the first start, NOT_DORMANT while the thread
    // runs, then what it returned or exited with
    uint32_t returned_value = SCE_KERNEL_ERROR_DORMANT;
    Ending ending = Ending::none;
    // Run the thread start event once before guest code. Set by start().
    bool run_start_callback = false;

    // Priority and affinity
    int priority;
    SceInt32 affinity_mask;

    // Waits
    // What the thread waits on while waiting, empty otherwise
    WaitTarget wait_target;
    // Set by wake() and consumed by the next wait().
    bool wake_pending = false;
    // A sceKernelSendSignal is pending for this thread.
    bool signal_pending = false;
    // The vblank the thread's last display wait ended on
    uint64_t last_vblank_waited = 0;

    // Callbacks
    // Callbacks of this thread with notifications they haven't run for.
    SceUInt32 notified_callbacks = 0;
    // Set while the thread runs its callbacks. They don't nest. With mutex held.
    bool is_processing_callbacks = false;
    // Callbacks this thread created, in creation order. The kernel owns them. Only this thread
    // touches the list.
    std::list<std::weak_ptr<Callback>> callbacks;

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

    // Threads blocked in sceKernelWaitThreadEnd on this one, with their own lock, taken after mutex
    std::mutex end_waiters_mutex;
    WaitQueue<EndWaitEntry> end_waiters;
};
