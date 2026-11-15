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
class Waitable : public WithClass<Waitable, KernelObject> {
public:
    // Returns the error for a name the guest can't use, or 0.
    static SceInt32 check_name(const char *name, SceUInt32 attr);

    // The attributes given at create, guarded by the lock. A message pipe rewrites its order bit.
    SceUInt32 attr;

    bool is_openable() const override { return openable; }
    // Locks the object, marks it deleted and runs end_waits()
    void on_delete(SceUID deleter) final;
    // Gives thread, which waits on this object, the priority and its place for it, and serves the
    // waiters, with the thread's lock held through thread_lock. Returns false with no change when
    // the object is locked, since its lock comes before the thread's.
    [[nodiscard]] bool try_requeue(ThreadState &thread, int priority, std::unique_lock<std::mutex> &thread_lock);

protected:
    Waitable(SceUInt32 attr, const char *name);

    // Locks the object's fields for its own methods, with an empty lock once it was deleted
    [[nodiscard]] std::unique_lock<std::mutex> lock() const {
        std::unique_lock<std::mutex> guard(mutex);
        if (deleted)
            guard.unlock();
        return guard;
    }

    // Runs the pending callbacks of a wait with callbacks and returns why it must end, or 0.
    [[nodiscard]] SceInt32 run_wait_callbacks(std::unique_lock<std::mutex> &lock, ThreadState &thread, bool callbacks);

    // Sleeps in the queue once with request, which the wakers read, and returns how the wait
    // ended, with the lock held.
    [[nodiscard]] SceInt32 wait(std::unique_lock<std::mutex> &lock, ThreadState &thread, SceUInt32 type, const WaitRequest &request, Deadline deadline, SceUInt32 *timeout);

    // Runs the callbacks notified during a wait that ended with result and returns the outcome.
    [[nodiscard]] SceInt32 finish_wait(std::unique_lock<std::mutex> &lock, ThreadState &thread, bool callbacks, SceInt32 result);

    // The queue methods are called with the lock held
    // Queues thread with request, which its wakers read, and shows the wait of this type on the object.
    void queue(ThreadState &thread, SceUInt32 type, const WaitRequest &request);
    // Takes thread off the queue with result if it is still on it, and clears the wait it shows.
    void unqueue(ThreadState &thread, SceInt32 result);
    // Links thread behind every waiter of its queued priority or a higher one, and behind a head that keeps its place.
    void insert_by_priority(ThreadState &thread);
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

    // The hooks of each class, in the order a wait reaches them, all called with the lock held
    // Runs before thread is linked into the queue.
    virtual void waiter_added(ThreadState &thread) {}
    // Returns whether thread queues behind the head whatever its priority.
    virtual bool queues_behind_head(const ThreadState &thread) const { return false; }
    // Runs after thread left the queue with result, whether woken or leaving on its own.
    virtual void wait_ended(ThreadState &thread, SceInt32 result) {}
    // Wakes the waiters that can now get what they wait for, in queue order.
    virtual void serve() {}
    // Ends the waits on the object when the thread deleter deletes it.
    virtual void end_waits(SceUID deleter) {}

    // The waiting threads in queue order, borrowed, guarded by the lock. Only queue() and try_requeue()
    // link one.
    IntrusiveList<ThreadState> waiters;

private:
    const bool openable;
    mutable std::mutex mutex;
    bool deleted = false;
};
