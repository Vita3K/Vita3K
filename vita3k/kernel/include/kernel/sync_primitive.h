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

#include <kernel/kernel_object.h>
#include <kernel/thread/wait_queue.h>
#include <kernel/types.h>

#include <mutex>

class SyncPrimitive : public KernelObject {
public:
    // Returns the error for a name the guest can't use, or 0.
    static SceInt32 check_name(const char *name, SceUInt32 attr);

    const SceUInt32 attr;

    bool is_openable() const override { return attr & SCE_KERNEL_ATTR_OPENABLE; }

protected:
    SyncPrimitive(SceUInt32 attr, const char *name);

    // Wakes the waiters that can now get what they wait for, in queue order. Called with the lock
    // held when the object gets something new to give, and by wait_loop() when a waiter leaves the
    // queue without being woken.
    virtual void serve() {}

    // Ends a queued wait of thread that its deadline ended, while it is still queued and before the
    // waiters behind it are served. Not called if the deadline passed before the wait was queued.
    virtual void timed_out(const ThreadStatePtr &) {}

    // When wait_loop() runs the callbacks notified during the wait, before it returns
    enum class CallbacksCheck {
        // Unless attempt succeeded
        after_wait,
        // Also when attempt succeeded
        after_take_or_wait,
        // Only when a delete ended the sleep
        on_delete,
    };

    // Runs the thread's pending callbacks if callbacks is set, releasing the lock while they run.
    // Returns WAIT_DELETE if the object was deleted meanwhile.
    [[nodiscard]] SceInt32 run_wait_callbacks(std::unique_lock<std::mutex> &lock, const ThreadStatePtr &thread, bool callbacks);

    // Waits on queue. Each round runs the pending callbacks, calls attempt, then sleeps until woken,
    // and a callback notify starts a new round. attempt returns the result once the thread got what
    // it waits for, or nullopt to keep waiting. It is passed the result of the last sleep
    // (SCE_KERNEL_OK the first time). timeout is updated after every sleep. callbacks_check decides
    // whether callbacks notified during the wait run before it returns.
    template <typename Entry, typename Attempt>
    [[nodiscard]] SceInt32 wait_loop(WaitQueue<Entry> &queue, std::unique_lock<std::mutex> &lock, const ThreadStatePtr &thread, WaitTarget target, Entry entry, SceUInt32 *timeout, CallbacksCheck callbacks_check, Attempt attempt);
};
