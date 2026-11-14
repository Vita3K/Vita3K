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

// A kernel object threads can wait on, with the lock that guards its fields
class Waitable : public KernelObject {
public:
    // Returns the error for a name the guest can't use, or 0.
    static SceInt32 check_name(const char *name, SceUInt32 attr);

    const SceUInt32 attr;

    bool is_openable() const override { return attr & SCE_KERNEL_ATTR_OPENABLE; }
    // Locks the object, marks it deleted and runs end_waits()
    void on_delete(SceUID deleter) final;

protected:
    Waitable(SceUInt32 attr, const char *name);

    // Locks the object's fields for its own methods, with an empty lock once it was deleted
    [[nodiscard]] std::unique_lock<std::mutex> lock() const {
        std::unique_lock<std::mutex> guard(mutex);
        if (deleted)
            guard.unlock();
        return guard;
    }

    // Ends the waits on the object when the thread deleter deletes it, with the lock held.
    virtual void end_waits(SceUID deleter) {}

    // Wakes the waiters that can now get what they wait for, in queue order, with the lock held.
    virtual void serve() {}

    // Runs when a waiter leaves the queue on its own with result, before serve().
    virtual void wait_end(ThreadState &thread, SceInt32 result) {}

    // Runs the pending callbacks of a wait with callbacks and returns why it must end, or 0.
    [[nodiscard]] SceInt32 run_wait_callbacks(std::unique_lock<std::mutex> &lock, ThreadState &thread, bool callbacks);

    // Sleeps in queue of this object once with request, which the wakers read, and returns how the
    // wait ended, with the lock held.
    [[nodiscard]] SceInt32 wait(WaitQueue &queue, std::unique_lock<std::mutex> &lock, ThreadState &thread, SceUInt32 type, const WaitRequest &request, Deadline deadline, SceUInt32 *timeout);

    // Runs the callbacks notified during a wait that ended with result and returns the outcome.
    [[nodiscard]] SceInt32 finish_wait(std::unique_lock<std::mutex> &lock, ThreadState &thread, bool callbacks, SceInt32 result);

    // The threads waiting on the object, guarded by the lock
    WaitQueue waiters;

private:
    mutable std::mutex mutex;
    bool deleted = false;
};
