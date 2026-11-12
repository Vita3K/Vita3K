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

#include <kernel/thread/wait.h>
#include <kernel/types.h>
#include <util/intrusive_list.h>

#include <concepts>

// Only declared here so ThreadState can hold a WaitQueue
struct ThreadState;

// Threads blocked on an object, in FIFO or thread priority order. Every method must be called with
// the object's lock held. The threads are linked in place, so queueing allocates nothing.
class WaitQueue {
public:
    // How the queue orders its waiters
    enum class Order {
        fifo,
        priority,
    };

    WaitQueue() = default;
    explicit WaitQueue(SceUInt32 attr)
        : by_priority(attr & SCE_KERNEL_ATTR_TH_PRIO) {}
    explicit WaitQueue(Order order)
        : by_priority(order == Order::priority) {}

    // The threads in queue order
    auto begin() const {
        return threads.begin();
    }
    auto end() const {
        return threads.end();
    }

    bool empty() const {
        return threads.empty();
    }

    std::size_t size() const {
        return threads.size();
    }

    // Returns the thread that would be woken first, or null.
    ThreadState *front();

    // Returns whether a thread of this priority would be first in the queue, ahead of every waiter.
    bool first_for(int priority) const;

    // Returns the first waiting thread accepted by pick, or null.
    ThreadState *find_if(std::predicate<ThreadState &> auto pick);

    // Ends a wait with result and takes the thread off the queue.
    void wake(ThreadState &thread, SceInt32 result = SCE_KERNEL_OK);

    // Ends the wait of every thread pick accepts, in queue order, and returns how many were woken.
    std::size_t wake_if(std::predicate<ThreadState &> auto pick, SceInt32 result = SCE_KERNEL_OK);

    // Ends the wait of every thread with result and returns how many were woken.
    std::size_t wake_all(SceInt32 result = SCE_KERNEL_OK);

    // Queues thread with request, which its wakers read.
    void push(ThreadState &thread, const WaitRequest &request);

    // Takes a thread off the queue if it is still on it.
    void remove(ThreadState &thread);

private:
    bool by_priority = false;
    IntrusiveList<ThreadState> threads;
};
