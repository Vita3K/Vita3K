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
#include <util/semaphore.h>

#include <condition_variable>
#include <mutex>
#include <string>

struct CPUState;
struct CPUContext;

typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::unique_ptr<CPUContext, std::function<void(CPUContext *)>> CPUContextPtr;

enum class ThreadToDo {
    exit,
    run,
    step,
    wait,
};

struct ThreadState {
    Block stack;
    int priority;
    int stack_size;
    CPUStatePtr cpu;
    CPUContextPtr cpu_context;
    Ptr<void> fiber;
    ThreadToDo to_do = ThreadToDo::run;
    std::mutex mutex;
    sync_utils::Semaphore signal;
    std::condition_variable something_to_do;
    std::vector<std::shared_ptr<ThreadState>> waiting_threads;
    std::string name;
    SceUID id;
    Address entry_point;
    int returned_value;
};

typedef std::shared_ptr<ThreadState> ThreadStatePtr;