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
#include <expected>
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
    // Fixed when the thread is created
    const SceUID id;
    const std::string name;
    const Address entry_point;
    // What thread info reports, the creation attributes and THREAD_ATTR_USER
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
    // The registers each run starts with
    const CPUContext init_cpu_ctx;

    ThreadState() = delete;
    // A dormant thread with its stack and TLS, which runs on cpu
    ThreadState(SceUID id, KernelState &kernel, MemState &mem, CPUStatePtr cpu, const char *name, Address entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, SceUInt32 attr, const SceKernelThreadOptParam *option);

    // Lifecycle
    int start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback = false);
    // Ends the thread like sceKernelExitThread: the end handler runs, then it parks dormant
    void exit(SceInt32 status);
    // Ends the thread like sceKernelExitDeleteThread: the end handler runs, then it's deleted
    void exit_delete(SceInt32 status);
    // Deletes the thread like sceKernelDeleteThread, or returns NOT_DORMANT if it isn't dormant
    SceInt32 remove_dormant();
    // Deletes a dormant thread, or one being torn down, without running its end handler
    void remove();
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

    // Status and info
    ThreadStatus get_status() const;
    // Whether the thread is deleted, so guest calls can't find it
    bool is_deleted();
    // The status a thread that ended exited with, or NOT_DORMANT or DORMANT if it hasn't ended
    std::expected<uint32_t, SceInt32> exit_status() const;
    // The thread's info, as sceKernelGetThreadInfo reports it
    SceKernelThreadInfo info();
    // The room left on the stack, all of it for a dormant thread
    int stack_free_size() const;
    Address stack_top() const;
    std::string log_stack_traceback() const;

    // Priority and affinity
    // The priority the thread runs at
    int get_priority() const;
    // Changes the priority the thread runs at, and the one its TLS shows. Returns the old priority.
    int change_priority(int new_priority);
    // The cores the thread may run on
    SceInt32 get_affinity_mask() const;
    // Changes the cores the thread may run on, and the ones its TLS shows. Returns the old mask.
    SceInt32 change_affinity_mask(SceInt32 new_affinity_mask);

    // Waits
    // Blocks this thread until the deadline passes.
    [[nodiscard]] WaitResult delay_until(Deadline deadline, bool callbacks);
    // Blocks this thread until a signal is sent to it or the timeout passes, writing the time left back.
    [[nodiscard]] WaitResult wait_for_signal(SceUInt32 *timeout, bool callbacks);
    // Sends a signal to this thread. Fails if the previous one was not consumed yet.
    SceInt32 send_signal();
    // Blocks waiter until this thread becomes dormant, then writes its exit status to exit_status.
    [[nodiscard]] WaitResult wait_for_thread_end(const ThreadStatePtr &waiter, SceInt32 *exit_status, Deadline deadline, bool callbacks);
    // Waits on target until woken by wake(), the thread exits or is deleted, or the deadline passes.
    // With target.callbacks, a notified callback ends it with NOTIFY_CALLBACK, and the caller runs
    // it with run_callbacks(). A stale wake can end it early, so callers must recheck their condition.
    [[nodiscard]] WaitResult wait(WaitTarget target, Deadline deadline);
    // Runs the notified callbacks for a wait that ended with NOTIFY_CALLBACK, with no lock held.
    // Returns ThreadExiting if the thread exits or is deleted meanwhile.
    [[nodiscard]] WaitResult run_callbacks();
    // Wakes this thread from wait().
    void wake();
    // Shows the thread WAITING while a host call blocks it, such as an audio output, and RUNNING after
    void begin_host_wait();
    void end_host_wait();
    // The vblank a display wait for vcount vblanks ends on: vcount after the one the thread's last
    // display wait ended on, and at least next_vblank. The next display wait counts from it.
    uint64_t next_display_wait(uint64_t next_vblank, int vcount);

    // Callbacks
    // Runs the notified callbacks of this thread and returns how many ran. Called by the thread itself.
    SceUInt32 process_callbacks();
    // Returns whether a notified callback would run now, which none does inside a callback. Takes mutex.
    bool has_pending_callbacks();
    // Adds a callback this thread created. Called by the thread itself.
    void add_callback(const CallbackPtr &cb);
    // Counts a callback of this thread that was notified and hasn't run for it yet, waking a wait
    // with callbacks, and stops counting it. Called by the callback with its lock held.
    void add_notified_callback();
    void remove_notified_callback();

    // Holds: the debugger, the emulator pause and the ForVM suspend
    // Stops the thread for the debugger, the next time it would run guest code
    void suspend();
    // Lets a thread the debugger stopped run again, for one instruction with step. Returns whether it
    // runs, which it doesn't while something else holds it. A step then doesn't happen at all.
    bool resume(bool step = false);
    // Waits until the debugger's hold has stopped the thread, or it's dormant or deleted
    void wait_until_stopped_for_debugger();
    // Waits until the thread doesn't run guest code
    void wait_until_not_running();
    // Whether the debugger's hold stopped the thread at a breakpoint
    bool stopped_at_breakpoint() const;
    // Stops the thread while the emulator is paused, and lets it run again after
    void pause();
    void resume_from_pause();
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

    struct EndWaitEntry {
        // Where to write the exit status, or null
        SceInt32 *exit_status;
    };

    // The private methods are called with mutex held, unless they say otherwise
    void update_status(ThreadStatus status, std::optional<ThreadStatus> expected = std::nullopt);
    // Whether the thread is ending
    bool exiting() const { return ending != Ending::none; }
    // Asks the thread to end, keeping a stronger request already made
    void request_ending(Ending how) { ending = std::max(ending, how); }
    // Ends the thread without its end handler, whatever its status
    void request_removal();
    // Set the priority or the affinity mask and the TLS slot that shows it
    void set_priority(int new_priority);
    void set_affinity_mask(SceInt32 new_affinity_mask);
    // Starts a dormant thread at entry, like start() does at its entry point. Takes mutex.
    int start_at(Address entry, SceSize arglen, Ptr<void> argp, bool run_entry_callback);
    // Whether a callback of this thread was notified and hasn't run yet
    bool callbacks_notified() const { return notified_callbacks > 0; }
    // Whether a notified callback ends the current wait. Callbacks don't nest, so a wait inside a
    // callback runs none. With mutex held.
    bool callback_ends_wait() const { return wait_target.callbacks && !is_processing_callbacks && callbacks_notified(); }
    // Whether the thread's current wait is over, by a wake, its end or a callback it runs, though
    // the thread hasn't gone on yet. With mutex held.
    bool wait_over() const { return exiting() || wake_pending || callback_ends_wait(); }
    // Keeps the thread from going on into guest code while anything holds it, unless it's ending.
    // With mutex held through lock.
    void wait_while_held(std::unique_lock<std::mutex> &lock, StopPoint where);
    // Whether the debugger's hold stopped the thread
    bool stopped_for_debugger() const { return holds.debugger && status == ThreadStatus::suspended; }
    // Whether the thread is inside a kernel call: waiting, or held on its way back from one
    bool in_wait() const { return status == ThreadStatus::waiting || (status == ThreadStatus::suspended && stop_point == StopPoint::syscall); }
    // Whether vm_suspend holds the thread
    bool vm_suspended() const { return holds.vm > 0; }
    // Waits, with mutex held through lock, until the thread no longer runs or vm_suspend no longer holds it
    void wait_for_vm_stop(std::unique_lock<std::mutex> &lock);
    // Called by the thread itself
    void push_arguments(const std::vector<uint32_t> &args);
    void dispatch_abort(CPUState &cpu);

    // Fixed when the thread is created
    KernelState &kernel;
    MemState &mem;

    mutable std::mutex mutex;

    // The rest is guarded by mutex, unless it says otherwise

    // Lifecycle
    ThreadStatus status = ThreadStatus::dormant;
    // The exit status the guest sees: DORMANT until the first start, NOT_DORMANT while the thread
    // runs, then what it returned or exited with
    uint32_t returned_value = SCE_KERNEL_ERROR_DORMANT;
    Ending ending = Ending::none;
    // when calling sceKernelStartThread
    bool run_start_callback = false;
    // Notified whenever the status or a hold changes
    std::condition_variable status_cond;

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
    // Notified whenever a condition a wait may be blocked on changes.
    std::condition_variable wait_cv;

    // Callbacks
    // Callbacks of this thread with notifications they haven't run for.
    SceUInt32 notified_callbacks = 0;
    // Set while the thread runs its callbacks. They don't nest. With mutex held.
    bool is_processing_callbacks = false;
    // Callbacks this thread created, in creation order. The kernel owns them. Only this thread
    // touches the list.
    std::list<std::weak_ptr<Callback>> callbacks;

    // Holds
    Holds holds;
    // The debugger asked for one instruction, after which the thread holds again
    bool step_requested = false;
    // Where the thread stopped the last time something held it
    StopPoint stop_point = StopPoint::guest_code;

    // Threads blocked in sceKernelWaitThreadEnd on this one, with their own lock, taken after mutex
    std::mutex end_waiters_mutex;
    WaitQueue<EndWaitEntry> end_waiters;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;
