// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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
#include <kernel/types.h>
#include <list>
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

struct ThreadJob {
    CPUContext ctx;
    std::function<void(int res)> notify;
    bool in_progress = false;
};

typedef std::list<ThreadJob> RunQueue;

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
    exit,
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
    uint64_t start_tick;

    CPUStatePtr cpu;
    ThreadStatus status = ThreadStatus::dormant;
    RunQueue run_queue;

    ThreadSignal signal;
    std::condition_variable status_cond;
    std::vector<std::shared_ptr<ThreadState>> waiting_threads;
    int returned_value;

    int init(KernelState &kernel, MemState &mem, const char *name, Ptr<const void> entry_point, int init_priority, int stack_size, const SceKernelThreadOptParam *option);
    int start(KernelState &kernel, MemState &mem, SceSize arglen, const Ptr<void> &argp);

    void update_status(ThreadStatus status, std::optional<ThreadStatus> expected = std::nullopt);
    Address stack_top() const;

    bool run_loop();
    void clear_run_queue();
    void stop_loop();
    void flush_callback_requests();
    void raise_waiting_threads();

    int run_guest_function(Address callback_address, const std::vector<uint32_t> &args);
    void request_callback(Address callback_address, const std::vector<uint32_t> &args, const std::function<void(int res)> notify = nullptr);

    void suspend();
    void resume(bool step = false);
    std::string log_stack_traceback(KernelState &kernel, MemState &mem) const;

private:
    CPUContext init_cpu_ctx;
    RunQueue callback_requests;
    ThreadToDo to_do = ThreadToDo::wait;
    std::condition_variable something_to_do;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;