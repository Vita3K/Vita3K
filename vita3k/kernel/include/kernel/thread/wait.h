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

// Returned instead of a result when the waiting thread is being deleted. The guest never sees it.
struct ThreadExiting {};

// Result of a wait: a guest return code, or ThreadExiting.
class [[nodiscard]] WaitResult {
public:
    WaitResult(SceInt32 code)
        : code(code) {}
    WaitResult(ThreadExiting)
        : exiting(true) {}

    // False when the thread is exiting.
    explicit operator bool() const {
        return !exiting;
    }

    // The guest return code, only valid when the thread is not exiting.
    SceInt32 operator*() const {
        return code;
    }

private:
    SceInt32 code = SCE_KERNEL_OK;
    bool exiting = false;
};

using Deadline = std::chrono::steady_clock::time_point;

// Converts a guest timeout in microseconds into a deadline. A null timeout waits forever.
inline Deadline deadline_from(const SceUInt32 *timeout) {
    if (!timeout)
        return Deadline::max();
    return std::chrono::steady_clock::now() + std::chrono::microseconds(*timeout);
}

// Writes the time left before the deadline back to a guest timeout.
inline void writeback_timeout(SceUInt32 *timeout, Deadline deadline) {
    if (!timeout || deadline == Deadline::max())
        return;
    const auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
    *timeout = left > 0 ? static_cast<SceUInt32>(left) : 0;
}

// Turns a wait result into the value returned to the guest.
// ThreadExiting becomes SCE_KERNEL_OK, which the guest never sees: after the HLE call returns,
// run_loop sees delete_requested and stops the thread before the next guest instruction.
inline SceInt32 guest_result(WaitResult r) {
    return r ? *r : SCE_KERNEL_OK;
}
