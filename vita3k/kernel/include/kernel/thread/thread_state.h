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

#include <cpu/state.h>
#include <kernel/callback.h>
#include <kernel/thread/wait_queue.h>
#include <kernel/types.h>
#include <mem/block.h>
#include <mem/ptr.h>

#include <algorithm>
#include <condition_variable>
#include <list>
#include <mutex>
#include <optional>
#include <string>

struct CPUContext;

struct ThreadState;
struct ThreadParams;
struct KernelState;

typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::function<void(CPUState &, uint32_t, SceUID)> CallImport;
typedef std::function<std::string(Address)> ResolveNIDName;

// The attribute of every user thread, which thread info always reports
constexpr SceUInt32 THREAD_ATTR_USER = 0x80000000U;
// Attributes a thread may be created with, which the Vita Development Wiki calls
// SCE_KERNEL_THREAD_ATTR_NOUSE_VFP and SCE_KERNEL_THREAD_ATTR_VM
constexpr SceUInt32 THREAD_ATTR_NOUSE_VFP = 0x01000000U;
constexpr SceUInt32 THREAD_ATTR_VM = 0x04000000U;
// Another attribute a Vita accepts for a thread, with no known name or effect
constexpr SceUInt32 THREAD_ATTR_UNKNOWN_2000 = 0x00002000U;
// The FPSCR a thread starts with: round to nearest, flush-to-zero and default NaN
constexpr uint32_t INITIAL_FPSCR = 0x03000000U;

// Values are what sceKernelGetThreadInfo reports
enum class ThreadStatus : SceUInt32 {
    running = SCE_KERNEL_THREAD_STATUS_RUNNING,
    waiting = SCE_KERNEL_THREAD_STATUS_WAITING, // Waiting to be awaken by sync object or operation
    dormant = SCE_KERNEL_THREAD_STATUS_DORMANT, // Waiting for a job
    deleted = SCE_KERNEL_THREAD_STATUS_DELETED, // Gone for guest calls, until its host thread ends
    suspended = SCE_KERNEL_THREAD_STATUS_SUSPENDED, // Suspended by debugger
};

struct ThreadState {
    std::mutex mutex;
    std::string name;
    SceUID id;
    Address entry_point;

    Block stack;
    int stack_size;
    Block tls;

    // What thread info reports, the creation attributes and THREAD_ATTR_USER
    SceUInt32 attr = THREAD_ATTR_USER;
    int priority;
    SceInt32 affinity_mask;
    uint64_t last_vblank_waited;

    CPUStatePtr cpu;
    ThreadStatus status = ThreadStatus::dormant;
    // What the thread waits on while waiting, empty otherwise
    WaitTarget wait_target;

    std::condition_variable status_cond;
    // The exit status the guest sees: DORMANT until the first start, NOT_DORMANT while the thread
    // runs, then what it returned or exited with
    uint32_t returned_value = SCE_KERNEL_ERROR_DORMANT;

    ThreadState() = delete;
    explicit ThreadState(SceUID id, KernelState &kernel, MemState &mem);

    int init(const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, const SceKernelThreadOptParam *option);
    int start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback = false);
    // Ends the thread like sceKernelExitThread: the end handler runs, then it parks dormant
    void exit(SceInt32 status);
    // Ends the thread like sceKernelExitDeleteThread: the end handler runs, then it's deleted
    void exit_delete(SceInt32 status);
    // Deletes a dormant thread, or one being torn down, without running its end handler
    void remove();

    void update_status(ThreadStatus status, std::optional<ThreadStatus> expected = std::nullopt);
    // Whether the thread is deleted, so guest calls can't find it
    bool is_deleted();
    Address stack_top() const;

    // Runs the thread's guest code. The host thread runs the top level, which parks the thread
    // dormant between starts until it's deleted. A callback runs a nested level, which returns
    // when the callback does or the thread ends.
    void run_loop(bool top_level = true);

    // this function must be called from the thread itself (inside a svc call)
    // With fresh_vfp_state, the call starts with the FPSCR and VFP exception setting of a new thread
    uint32_t run_callback(Address callback_address, const std::vector<uint32_t> &args, bool fresh_vfp_state = false);

    // this function is called from another thread when this one is dormant
    // it is only used for module loading and gxm display queue right now
    // args and argp are passed to thread->start as is
    uint32_t run_guest_function(Address callback_address, SceSize args = 0, const Ptr<void> argp = Ptr<void>{});

    // Blocks this thread until the deadline passes.
    [[nodiscard]] WaitResult delay_until(Deadline deadline, bool callbacks);
    // Blocks this thread until a signal is sent to it or the timeout passes, writing the time left back.
    [[nodiscard]] WaitResult wait_for_signal(SceUInt32 *timeout, bool callbacks);
    // Sends a signal to this thread. Fails if the previous one was not consumed yet.
    SceInt32 send_signal();
    // Blocks waiter until this thread becomes dormant, then writes its exit status to exit_status.
    [[nodiscard]] WaitResult wait_for_thread_end(const ThreadStatePtr &waiter, SceInt32 *exit_status, Deadline deadline, bool callbacks);

    // Waits on target until woken by wake(), the thread exits or is deleted, or the deadline passes.
    // With callbacks, it also returns after running callbacks that were notified meanwhile.
    // A stale wake or callbacks can end it early, so callers must recheck their condition.
    [[nodiscard]] WaitResult wait(WaitTarget target, Deadline deadline, bool callbacks);
    // Wakes this thread from wait().
    void wake();

    // Runs the notified callbacks of this thread and returns how many ran. Called by the thread itself.
    SceUInt32 process_callbacks();
    // Adds a callback this thread created. Called by the thread itself.
    void add_callback(const CallbackPtr &cb);
    // Counts a callback of this thread that was notified and hasn't run for it yet, waking a wait
    // with callbacks, and stops counting it. Called by the callback with its lock held.
    void add_notified_callback();
    void remove_notified_callback();
    // Whether a callback of this thread was notified and hasn't run yet. Called with mutex held.
    bool callbacks_notified() const { return notified_callbacks > 0; }

    // Stops the thread for the debugger, the next time it would run guest code
    void suspend();
    // Lets a thread the debugger stopped run again, for one instruction with step. Returns whether it
    // runs, which it doesn't while something else holds it.
    bool resume(bool step = false);
    // Whether the thread has stopped for the debugger, where suspend() stops it. Called with mutex held.
    bool stopped_for_debugger() const { return holds.debugger && status == ThreadStatus::suspended; }
    // Stops the thread while the emulator is paused, and lets it run again after
    void pause();
    void resume_from_pause();
    // Suspends the thread for sceKernelSuspendThreadForVM: it doesn't run guest code until as many
    // vm_resume calls. vm_suspend returns once it has stopped. Each returns the suspend count after
    // it, or an error for a dormant thread or a resume with no suspend.
    int vm_suspend();
    int vm_resume();
    // Whether vm_suspend holds the thread. Called with mutex held.
    bool vm_suspended() const { return holds.vm > 0; }
    // Waits, with mutex held through lock, until the thread no longer runs or vm_suspend no longer holds it
    void wait_for_vm_stop(std::unique_lock<std::mutex> &lock);
    // Whether the thread is inside a kernel call: waiting, or held on its way back from one. Called
    // with mutex held.
    bool in_wait() const { return status == ThreadStatus::waiting || (status == ThreadStatus::suspended && stop_point == StopPoint::syscall); }
    std::string log_stack_traceback() const;

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

    // What keeps the thread from running guest code. It runs only while nothing holds it.
    struct Holds {
        // The emulator is paused
        bool pause = false;
        // The debugger stopped it, or it hit a breakpoint or finished a step
        bool debugger = false;
        // The sceKernelSuspendThreadForVM calls no sceKernelResumeThreadForVM undid yet
        int vm = 0;

        bool any() const { return pause || debugger || vm > 0; }
        // Whether the guest sees the thread as SUSPENDED. A pause alone doesn't show.
        bool shown() const { return debugger || vm > 0; }
    };

    // Where a held thread stopped
    enum class StopPoint : uint8_t {
        // In its own code
        guest_code,
        // On its way back from a call into the kernel
        syscall,
    };

    // Whether the thread is ending. Called with mutex held.
    bool exiting() const { return ending != Ending::none; }
    // Keeps the thread from going on into guest code while anything holds it, unless it's ending.
    // Called with mutex held through lock.
    void wait_while_held(std::unique_lock<std::mutex> &lock, StopPoint where);
    // Asks the thread to end, keeping a stronger request already made. Called with mutex held.
    void request_ending(Ending how) { ending = std::max(ending, how); }

    void push_arguments(const std::vector<uint32_t> &args);
    void dispatch_abort(CPUState &cpu);

    KernelState &kernel;

    CPUContext init_cpu_ctx;
    Ending ending = Ending::none;
    Holds holds;
    // The debugger asked for one instruction, after which the thread holds again
    bool step_requested = false;
    // Where the thread stopped the last time something held it
    StopPoint stop_point = StopPoint::guest_code;

    // when calling sceKernelStartThread
    bool run_start_callback = false;

    MemState &mem;

    // A sceKernelSendSignal is pending for this thread.
    bool signal_pending = false;
    // Set by wake() and consumed by the next wait().
    bool wake_pending = false;
    // Callbacks of this thread with notifications they haven't run for.
    SceUInt32 notified_callbacks = 0;
    // Set while the thread runs its callbacks. They don't nest.
    bool is_processing_callbacks = false;
    // Callbacks this thread created, in creation order. The kernel owns them. Only this thread touches the list.
    std::list<std::weak_ptr<Callback>> callbacks;

    // Notified under mutex whenever a condition a wait may be blocked on changes.
    std::condition_variable wait_cv;

    struct EndWaitEntry {
        // Where to write the exit status, or null
        SceInt32 *exit_status;
    };

    // Guards end_waiters. Taken after mutex when both are needed.
    std::mutex end_waiters_mutex;
    // Threads blocked in sceKernelWaitThreadEnd on this one.
    WaitQueue<EndWaitEntry> end_waiters;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;
