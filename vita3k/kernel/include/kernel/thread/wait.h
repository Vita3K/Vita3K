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

#include <kernel/types.h>

#include <chrono>

using Deadline = std::chrono::steady_clock::time_point;

// What a waiting thread is blocked on, as sceKernelGetThreadInfo reports it.
struct WaitTarget {
    // One of SCE_KERNEL_WAITTYPE_*, or 0 if not waiting, with SCE_KERNEL_WAITTYPE_CB_BIT for a wait
    // with callbacks
    SceUInt32 type = 0;
    // UID of the object waited on, or -1 if there is none, like for a delay
    SceUID id = -1;
};

struct MsgPipeSender;
struct MsgPipeReceiver;

// What a waiting thread asked for, which the wakers of the object it waits on read
struct WaitRequest {
    // The semaphore count it needs, or the mutex lock count it takes
    SceInt32 count = 0;
    // An rwlock write lock
    bool write = false;
    // The event or event flag bits it waits for
    SceUInt32 pattern = 0;
    // The event flag wait mode
    SceUInt32 mode = 0;
    // Where the event or event flag bits go, or null
    SceUInt32 *out_pattern = nullptr;
    // Where the event user data goes, or null
    SceUInt64 *out_user_data = nullptr;
    // Where the exit status of the thread it waits for goes, or null
    SceInt32 *out_status = nullptr;
    // The message pipe transfer of a send, or null
    MsgPipeSender *sender = nullptr;
    // The message pipe transfer of a receive, or null
    MsgPipeReceiver *receiver = nullptr;
    // The vblank count a display wait waits for
    uint64_t vcount = 0;
};

// Returns the wait type with the callback bit if callbacks is set.
constexpr SceUInt32 wait_type(SceUInt32 type, bool callbacks) {
    return callbacks ? type | SCE_KERNEL_WAITTYPE_CB_BIT : type;
}

// Converts a guest timeout in microseconds into a deadline. A null timeout waits forever.
inline Deadline deadline_from(const SceUInt32 *timeout) {
    if (!timeout)
        return Deadline::max();
    return std::chrono::steady_clock::now() + std::chrono::microseconds(*timeout);
}

// Returns the microseconds left before the deadline, 0 once it passed.
inline SceUInt32 time_left_until(Deadline deadline) {
    const auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
    return left > 0 ? static_cast<SceUInt32>(left) : 0;
}

// Writes the time left before the deadline back to a guest timeout.
inline void writeback_timeout(SceUInt32 *timeout, Deadline deadline) {
    if (!timeout || deadline == Deadline::max())
        return;
    *timeout = time_left_until(deadline);
}
