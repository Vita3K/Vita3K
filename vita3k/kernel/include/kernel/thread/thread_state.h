// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <condition_variable>
#include <cpu/state.h>
#include <kernel/callback.h>
#include <kernel/types.h>
#include <mem/block.h>
#include <mem/ptr.h>
#include <mutex>
#include <optional>
#include <string>

struct CPUState;
struct CPUContext;

struct ThreadState;
struct ThreadParams;
struct KernelState;

typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::function<void(CPUState &, uint32_t, SceUID)> CallImport;
typedef std::function<std::string(Address)> ResolveNIDName;

enum class ThreadStatus {
    run, // Running
    dormant, // Waiting for a job
    suspend, // Suspended by debugger
    wait, // Waiting to be awaken by sync object or operation
};

struct ThreadSignal {
    ThreadSignal() = default;
    ~ThreadSignal() = default;

    void wait();
    bool send();

private:
    std::mutex mutex;
    std::condition_variable recv_cond;
    bool signaled = false;
};

// Internal
enum class ThreadToDo {
    remove,
    run,
    step,
    suspend,
    wait,
};

struct ThreadState {
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
    bool is_processing_callbacks = false;

    CPUStatePtr cpu;
    ThreadStatus status = ThreadStatus::dormant;

    ThreadSignal signal;
    std::vector<CallbackPtr> callbacks;
    std::condition_variable status_cond;
    std::vector<std::shared_ptr<ThreadState>> waiting_threads;
    uint32_t returned_value = 0;

    ThreadState() = delete;
    explicit ThreadState(SceUID id, KernelState &kernel, MemState &mem);

    int init(const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, const SceKernelThreadOptParam *option);
    int start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback = false);
    void exit(SceInt32 status);
    void exit_delete(bool exit = true);

    void update_status(ThreadStatus status, std::optional<ThreadStatus> expected = std::nullopt);
    Address stack_top() const;

    bool run_loop();
    void raise_waiting_threads();

    // this function must be called from the thread itself (inside a svc call)
    uint32_t run_callback(Address callback_address, const std::vector<uint32_t> &args);

    // this function is called from another thread when this one is dormant
    // it is only used for module loading and gxm display queue right now
    // args and argp are passed to thread->start as is
    uint32_t run_guest_function(Address callback_address, SceSize args = 0, const Ptr<void> argp = Ptr<void>{});

    void suspend();
    void resume(bool step = false);
    std::string log_stack_traceback() const;

private:
    void push_arguments(const std::vector<uint32_t> &args);

    KernelState &kernel;

    CPUContext init_cpu_ctx;
    ThreadToDo to_do = ThreadToDo::wait;
    std::condition_variable something_to_do;

    // if looking at the thread stack, the number of times run_loop appear
    // if the thread is dormant, call_level is 0
    // most of the time the thread is running, call_level is 1
    // if running a callback inside a callback (possible for exemple by allocating
    // gxm callbacked memory inside a kernel callback), call_level is 2
    int call_level = 0;

    // when calling sceKernelStartThread
    bool run_start_callback = false;
    // when calling sceKernelExitThread or sceKernelExitDeleteThread
    bool run_end_callback = false;

    MemState &mem;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;
