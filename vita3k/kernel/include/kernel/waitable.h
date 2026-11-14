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
#include <kernel/thread/wait.h>
#include <kernel/types.h>
#include <util/intrusive_list.h>

#include <concepts>
#include <mutex>

struct ThreadState;

// A kernel object threads can wait on, with the lock that guards its fields and its waiters
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

    // Sleeps in the queue once with request, which the wakers read, and returns how the wait
    // ended, with the lock held.
    [[nodiscard]] SceInt32 wait(std::unique_lock<std::mutex> &lock, ThreadState &thread, SceUInt32 type, const WaitRequest &request, Deadline deadline, SceUInt32 *timeout);

    // Runs the callbacks notified during a wait that ended with result and returns the outcome.
    [[nodiscard]] SceInt32 finish_wait(std::unique_lock<std::mutex> &lock, ThreadState &thread, bool callbacks, SceInt32 result);

    // The queue methods are called with the lock held
    // Queues thread with request, which its wakers read.
    void queue(ThreadState &thread, const WaitRequest &request);
    // Takes thread off the queue if it is still on it.
    void unqueue(ThreadState &thread);
    // Returns the first waiter, or null.
    ThreadState *first_waiter();
    // Returns whether a new waiter of this priority would be first, ahead of every waiter.
    bool first_for(int priority) const;
    // Returns the first waiter pick accepts, or null.
    ThreadState *find_waiter(std::predicate<ThreadState &> auto pick);
    // Ends the wait of thread with result and takes it off the queue.
    void wake_waiter(ThreadState &thread, SceInt32 result = SCE_KERNEL_OK);
    // Ends the wait of every waiter pick accepts, in queue order, and returns how many.
    std::size_t wake_waiters_if(std::predicate<ThreadState &> auto pick, SceInt32 result = SCE_KERNEL_OK);
    // Ends the wait of every waiter with result and returns how many.
    std::size_t wake_waiters(SceInt32 result = SCE_KERNEL_OK);

    // The waiting threads in queue order, borrowed, guarded by the lock. Only queue() links one.
    IntrusiveList<ThreadState> waiters;

private:
    mutable std::mutex mutex;
    bool deleted = false;
};
