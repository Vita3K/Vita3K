// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <vector>

struct CPUState;
struct ThreadState;
struct CPUDepInject;

typedef std::function<void(CPUState &, uint32_t, SceUID)> CallImport;
typedef std::function<std::string(Address)> ResolveNIDName;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;

SceUID create_thread(Ptr<const void> entry_point, KernelState &kernel, MemState &mem, const char *name, int init_priority, int stack_size, const SceKernelThreadOptParam *option);
int start_thread(KernelState &kernel, const SceUID &thid, SceSize arglen, const Ptr<void> &argp);
void exit_thread(ThreadState &thread);
void exit_and_delete_thread(ThreadState &thread);
void delete_thread(KernelState &kernel, ThreadState &thread);
int wait_thread_end(ThreadStatePtr &waiter, ThreadStatePtr &target, int *stat);
Ptr<void> copy_block_to_stack(ThreadState &thread, MemState &mem, const Ptr<void> &data, const int size);
int run_callback(KernelState &kernel, ThreadState &thread, const SceUID &thid, Address callback_address, const std::vector<uint32_t> &args);
uint32_t run_on_current(ThreadState &thread, const Ptr<const void> entry_point, SceSize arglen, const Ptr<void> &argp);
void raise_waiting_threads(ThreadState *thread);
