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

constexpr auto kernel_tls_size = 0x800;

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

    CPUStatePtr cpu;
    ThreadStatus status = ThreadStatus::dormant;
    RunQueue run_queue;

    ThreadSignal signal;
    std::condition_variable status_cond;
    std::vector<std::shared_ptr<ThreadState>> waiting_threads;
    int returned_value;

    static SceUID create(Ptr<const void> entry_point, KernelState &kernel, MemState &mem, const char *name, int init_priority, int stack_size, const SceKernelThreadOptParam *option);
    int start(KernelState &kernel, SceSize arglen, const Ptr<void> &argp);
    void update_status(ThreadStatus status, std::optional<ThreadStatus> expected = std::nullopt);
    bool run_loop();
    void flush_callback_requests();
    void halt();
    void exit();
    void raise_waiting_threads();
    Ptr<void> copy_block_to_stack(MemState &mem, const Ptr<void> &data, const int size);
    int run_guest_function(Address callback_address, const std::vector<uint32_t> &args);
    void request_callback(Address callback_address, const std::vector<uint32_t> &args, const std::function<void(int res)> notify = nullptr);

    void suspend();
    void resume(bool step = false);
    std::string log_stack_traceback(KernelState &kernel, MemState &mem);

private:
    void clear_run_queue();

    CPUContext init_cpu_ctx;
    RunQueue jobs_to_add;
    ThreadToDo to_do = ThreadToDo::wait;
    std::condition_variable something_to_do;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;