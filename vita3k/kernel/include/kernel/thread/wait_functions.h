// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <kernel/thread/wait_state.h>
#include <mem/ptr.h>

#include <memory>
#include <mutex>

struct KernelState;
struct MemState;
struct ThreadState;

uint64_t make_timeout_deadline_us(KernelState &kernel, uint32_t timeout_us);
uint32_t remaining_timeout_us(const WaitState &wait, uint64_t guest_now_us);
uint64_t dispatch_pre_wait_callbacks(KernelState &kernel, SceUID thread_id, bool callbacks_allowed);
bool wait_for_timeout_or_signal(KernelState &kernel, std::unique_lock<std::mutex> &thread_lock, const std::shared_ptr<ThreadState> &thread, WaitType expected_type, SceUID expected_object, uint32_t *timeout);

SceInt32 wait_thread_signal(KernelState &kernel, SceUID thread_id, uint32_t delay, uint32_t timeout, bool callbacks_allowed = false);
SceInt32 wait_thread_end(KernelState &kernel, MemState &mem, const std::shared_ptr<ThreadState> &waiter, const std::shared_ptr<ThreadState> &target, Ptr<int> stat, SceUInt *timeout, bool callbacks_allowed = false);
SceInt32 delay_thread(KernelState &kernel, SceUID thread_id, SceUInt delay_us, bool callbacks_allowed = false);
