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
#include <mutex>
#include <string>

struct CPUState;
struct CPUContext;

typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;

enum class ThreadToDo {
    exit_delete,
    exit,
    run,
    step,
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
    ThreadToDo to_do = ThreadToDo::run;
    std::mutex mutex;
    ThreadSignal signal;
    std::condition_variable something_to_do;
    std::vector<std::shared_ptr<ThreadState>> waiting_threads;
    std::string name;
    SceUID id;
    Address entry_point;
    int returned_value;
    Block tls;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;
