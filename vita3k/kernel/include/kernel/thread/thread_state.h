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

#include <mem/block.h>
#include <mem/ptr.h>

#include <condition_variable>
#include <list>
#include <mutex>
#include <string>

struct CPUState;
struct CPUContext;

typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;

struct ThreadJob {
    CPUContext ctx;
    std::function<void(int res)> notify;
    bool in_progress = false;
};

typedef std::list<ThreadJob> RunQueue;

enum class ThreadToDo {
    exit,
    run,
    step,
    wait,
};

enum class ThreadStatus {
    run,
    dormant,
    wait,
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

struct ThreadState {
    Block stack;
    int priority;
    int stack_size;
    int core_num;
    CPUStatePtr cpu;
    CPUContext init_cpu_ctx;
    RunQueue run_queue;
    RunQueue jobs_to_add;
    ThreadToDo to_do = ThreadToDo::wait;
    std::condition_variable something_to_do;
    std::mutex mutex;
    ThreadSignal signal;
    ThreadStatus status = ThreadStatus::dormant;
    std::condition_variable status_cond;
    std::vector<std::shared_ptr<ThreadState>> waiting_threads;
    std::string name;
    SceUID id;
    Address entry_point;
    int returned_value;
    Block tls;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;
