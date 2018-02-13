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
#include <mem/mem.h> // Address.
#include <mutex>

struct CPUState;
template <typename T>
class Resource;

typedef Resource<Address> ThreadStack;
typedef std::shared_ptr<ThreadStack> ThreadStackPtr;
typedef std::unique_ptr<CPUState, std::function<void(CPUState *)>> CPUStatePtr;

enum class ThreadToDo {
    exit,
    run,
    wait,
};

struct ThreadState {
    ThreadStackPtr stack;
    CPUStatePtr cpu;
    ThreadToDo to_do = ThreadToDo::run;
    std::mutex mutex;
    std::condition_variable something_to_do;
};
