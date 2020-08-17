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

#include <cpu/functions.h>
#include <kernel/state.h>
#include <mem/ptr.h>
//#include <psp2/types.h>

#include <functional>
#include <memory>

struct CPUState;
struct ThreadState;
struct CPUDepInject;

typedef std::function<void(CPUState &, uint32_t, SceUID)> CallImport;
typedef std::function<std::string(Address)> ResolveNIDName;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;

SceUID create_thread(Ptr<const void> entry_point, KernelState &kernel, MemState &mem, const char *name, int init_priority, int stack_size, CPUDepInject &inject, const SceKernelThreadOptParam *option);
int start_thread(KernelState &kernel, const SceUID &thid, SceSize arglen, const Ptr<void> &argp);
Ptr<void> copy_stack(SceUID thid, SceUID thread_id, const Ptr<void> &argp, KernelState &kernel, MemState &mem);
bool run_thread(ThreadState &thread);
void raise_waiting_threads(ThreadState *thread);

template <typename T>
uint32_t unpack(T v) {
    return v;
}

template <typename P>
uint32_t unpack(Ptr<P> ptr) {
    return ptr.address();
}

template <size_t i, typename T>
void write_args(CPUState &cpu, T v) {
    static_assert(i < 4);
    write_reg(cpu, i, unpack(v));
}

template <size_t i, typename T, typename... Args>
void write_args(CPUState &cpu, T first, Args... args) {
    write_args<i>(cpu, first);
    write_args<i + 1>(cpu, args...);
}

template <typename... Args>
uint32_t run_guest_function(ThreadState &thread, const Address &entry_point, Args... args) {
    std::unique_lock<std::mutex> lock(thread.mutex);
    const auto ctx = save_context(*thread.cpu);
    write_args<0>(*thread.cpu, args...);
    write_pc(*thread.cpu, entry_point);
    lock.unlock();
    run_thread(thread);
    load_context(*thread.cpu, ctx);
    return ctx.cpu_registers[0];
}
