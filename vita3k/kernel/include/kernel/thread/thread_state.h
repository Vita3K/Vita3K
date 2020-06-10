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

#include <mem/mem.h> // Address.
#include <mem/ptr.h>

#include <condition_variable>
#include <mutex>
#include <string>

struct CPUState;
struct CPUContext;

template <typename T>
class Resource;

struct InitialFiber;

typedef Resource<Address> ThreadStack;
typedef std::shared_ptr<ThreadStack> ThreadStackPtr;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;
typedef std::unique_ptr<CPUContext, std::function<void(CPUContext *)>> CPUContextPtr;
typedef std::vector<InitialFiber> InitialFibers;

enum class ThreadToDo {
    exit,
    run,
    step,
    wait,
};

struct SceFiber;

struct InitialFiber {
    Address start;
    Address end;
    SceFiber *fiber;
};

struct ThreadState {
    ThreadStackPtr stack;
    int priority;
    int stack_size;
    CPUStatePtr cpu;
    CPUContextPtr cpu_context;
    Ptr<void> fiber;
    InitialFibers initial_fibers;
    ThreadToDo to_do = ThreadToDo::run;
    std::mutex mutex;
    std::condition_variable something_to_do;
    std::vector<std::shared_ptr<ThreadState>> waiting_threads;
    std::string name;
    Address entry_point;
};
