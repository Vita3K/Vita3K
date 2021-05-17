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
#include <optional>
#include <vector>

struct CPUState;
struct ThreadState;
struct CPUDepInject;

typedef std::function<void(CPUState &, uint32_t, SceUID)> CallImport;
typedef std::function<std::string(Address)> ResolveNIDName;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;

ThreadStatePtr create_thread(KernelState &kernel, MemState &mem, const char *name);
SceUID create_thread(Ptr<const void> entry_point, KernelState &kernel, MemState &mem, const char *name, int init_priority, int stack_size, const SceKernelThreadOptParam *option);
int start_thread(KernelState &kernel, const SceUID &thid, SceSize arglen, const Ptr<void> &argp);
Ptr<void> copy_block_to_stack(ThreadState &thread, MemState &mem, const Ptr<void> &data, const int size);
bool is_running(KernelState &kernel, ThreadState &thread);
void update_status(ThreadState &thread, ThreadStatus status, std::optional<ThreadStatus> expected = std::nullopt);
void wait_thread_status(ThreadState &thread, ThreadStatus status);
void exit_thread(ThreadState &thread);
void exit_and_delete_thread(ThreadState &thread);
void delete_thread(KernelState &kernel, ThreadState &thread);
int wait_thread_end(ThreadStatePtr &waiter, ThreadStatePtr &target, int *stat);
void raise_waiting_threads(ThreadState *thread);

int run_guest_function(KernelState &kernel, ThreadState &thread, Address callback_address, const std::vector<uint32_t> &args);
void request_callback(ThreadState &thread, Address callback_address, const std::vector<uint32_t> &args, const std::function<void(int res)> notify = nullptr);