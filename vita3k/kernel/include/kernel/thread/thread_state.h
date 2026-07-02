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
#include <kernel/thread/wait_state.h>
#include <kernel/types.h>
#include <mem/block.h>
#include <mem/ptr.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct CPUContext;

struct ThreadState;
struct ThreadParams;
struct KernelState;
struct MemState;

typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::function<void(CPUState &, uint32_t, SceUID)> CallImport;
typedef std::function<std::string(Address)> ResolveNIDName;

class KernelThreadManager;

enum class SuspendType {
    Process,
    Thread,
    Debug,
    Init,
    System,
};

enum class RunState {
    Initialized,
    Waiting,
    Runnable,
    Terminated,
};

struct ThreadState {
    friend class KernelThreadManager;

    std::mutex mutex;
    std::string name;
    SceUID id;
    Address entry_point;

    Block stack;
    int stack_size;
    Block tls;

    int priority;
    SceInt32 affinity_mask;
    uint64_t start_tick;
    uint64_t last_vblank_waited;
    // set to true if thread is processing kernel callbacks
    std::atomic_bool is_processing_callbacks = false;

    CPUStatePtr cpu;
    RunState state = RunState::Initialized;
    WaitReason wait_reason = WaitReason::None;
    WaitState wait;
    InterruptedWaitState interrupted_wait;
    uint64_t wait_sequence = 0;
    uint64_t wait_sequence_counter = 0;
    uint32_t suspend_flags = 0;

    bool signal_pending = false;
    bool callback_wakeup_pending = false;
    std::vector<ThreadEndWaiterRef> thread_end_waiters;
    std::vector<CallbackPtr> callbacks;
    std::condition_variable state_cond;
    uint32_t returned_value = 0;

    ThreadState() = delete;
    explicit ThreadState(SceUID id, KernelState &kernel, MemState &mem);

    int init(const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, const SceKernelThreadOptParam *option);
    int start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback = false);
    void exit(SceInt32 status);
    void exit_delete(bool exit = true);
    Address stack_top() const;

    void run_loop();
    void complete_thread_end_waiters();

    // this function must be called from the thread itself (inside a svc call)
    uint32_t run_callback(Address callback_address, const std::vector<uint32_t> &args);

    // this function is called from another thread when this one is dormant
    // it is only used for module loading and gxm display queue right now
    // args and argp are passed to thread->start as is
    uint32_t run_guest_function(Address callback_address, SceSize args = 0, const Ptr<void> argp = Ptr<void>{});

    void suspend(SuspendType type);
    void suspend();
    void resume(SuspendType type, bool step = false);
    void resume(bool step = false);
    std::string log_stack_traceback() const;

    bool is_executing() const { return is_executing_guest; }
    bool has_wait_state() const { return wait.type != WaitType::None; }
    bool has_interrupted_wait_state() const { return interrupted_wait.valid && interrupted_wait.wait.type != WaitType::None; }
    bool is_dormant() const { return state == RunState::Initialized; }
    bool is_waiting() const { return state == RunState::Waiting; }
    bool can_dispatch_guest() const { return state == RunState::Runnable && suspend_flags == 0; }
    bool is_suspend_requested(SuspendType type) const;
    WaitState *find_wait_state_by_sequence(uint64_t sequence);
    const WaitState *find_wait_state_by_sequence(uint64_t sequence) const;
    bool is_interrupted_wait_sequence(uint64_t sequence) const;

private:
    void push_arguments(const std::vector<uint32_t> &args);
    void dispatch_abort(CPUState &cpu);

    KernelState &kernel;

    CPUContext init_cpu_ctx;
    // sceKernelExitThread (or top-level guest function return): park at dormant, thread reusable via start() / run_guest_function().
    bool exit_requested = false;
    // sceKernelExitDeleteThread (or external kill): will return from top-level run_loop(), then host thread joins.
    bool delete_requested = false;
    // Set by suspend(), consumed in run_loop() after the current guest slice stops.
    bool suspend_requested = false;
    // Single stepping mode.
    bool single_stepping = false;
    bool is_executing_guest = false;

    // Number of active run_loop frames. The top-level host thread keeps one
    // frame alive (run_loop()) while parked dormant; callbacks add nested frames.
    int call_level = 0;

    // when calling sceKernelStartThread
    bool run_start_callback = false;
    // when calling sceKernelExitThread or sceKernelExitDeleteThread
    bool run_end_callback = false;

    MemState &mem;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;
