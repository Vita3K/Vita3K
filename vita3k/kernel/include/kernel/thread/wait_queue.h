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

#include <algorithm>
#include <concepts>
#include <list>
#include <memory>
#include <mutex>
#include <optional>

// Only declared here so ThreadState can hold a WaitQueue. The template bodies need the full type.
struct ThreadState;
using ThreadStatePtr = std::shared_ptr<ThreadState>;

// Threads blocked on a sync object, in FIFO or thread priority order.
// Entry is the per-waiter data the object needs, such as a requested count.
// Every method must be called with the object's lock held.
template <typename Entry>
class WaitQueue {
public:
    struct Waiter {
        ThreadStatePtr thread;
        // Taken when the wait starts, the queue is not reordered if it changes
        int priority = 0;
        Entry entry;
        // Set by a waker that decided the outcome of the wait
        std::optional<SceInt32> result;
    };

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

    // Blocks thread on target until a waker gives it a result, it is being deleted, or the deadline passes.
    // thread must be the guest thread making this HLE call.
    // With target.callbacks, the thread runs its notified callbacks while it waits.
    [[nodiscard]] WaitResult wait(std::unique_lock<std::mutex> &lock, const ThreadStatePtr &thread, WaitTarget target, Entry entry, Deadline deadline) {
        Waiter waiter{ .thread = thread, .entry = std::move(entry) };
        waiter.priority = waiter.thread->get_priority();
        push(waiter);
        while (true) {
            lock.unlock();
            WaitResult r = waiter.thread->wait(target, deadline);
            if (r && *r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
                r = waiter.thread->run_callbacks();
            lock.lock();
            // A waker's result wins, it already handed over what the waiter asked for
            if (waiter.result)
                return *waiter.result;
            if (!r || *r == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
                remove(waiter);
                return r;
            }
        }
    }

    // The waiters in queue order
    auto begin() const {
        return waiters.begin();
    }
    auto end() const {
        return waiters.end();
    }

    bool empty() const {
        return waiters.empty();
    }

    std::size_t size() const {
        return waiters.size();
    }

    // Returns the waiter that would be woken first, or null.
    Waiter *front() {
        return waiters.empty() ? nullptr : waiters.front();
    }

    // Returns whether a thread of this priority would be first in the queue, ahead of every waiter.
    bool first_for(int priority) const {
        if (waiters.empty())
            return true;
        // Lower value is higher priority, and equal priorities keep FIFO order
        return by_priority && waiters.front()->priority > priority;
    }

    // Returns the first waiter accepted by pick, or null.
    Waiter *find_if(std::predicate<Waiter &> auto pick) {
        const auto it = std::find_if(waiters.begin(), waiters.end(), [&](Waiter *w) { return pick(*w); });
        return it == waiters.end() ? nullptr : *it;
    }

    // Returns how many waiters pick accepts.
    std::size_t count_if(std::predicate<const Waiter &> auto pick) const {
        return std::count_if(waiters.begin(), waiters.end(), [&](const Waiter *w) { return pick(*w); });
    }

    // Ends a wait with result and takes the waiter off the queue.
    void wake(Waiter &waiter, SceInt32 result = SCE_KERNEL_OK) {
        waiter.result = result;
        remove(waiter);
        waiter.thread->wake();
    }

    // Ends the wait of every waiter accepted by pick, in queue order. Returns how many were woken.
    std::size_t wake_if(std::predicate<Waiter &> auto pick, SceInt32 result = SCE_KERNEL_OK) {
        std::size_t woken = 0;
        for (auto it = waiters.begin(); it != waiters.end();) {
            Waiter &waiter = **it;
            if (!pick(waiter)) {
                ++it;
                continue;
            }
            it = waiters.erase(it);
            waiter.result = result;
            waiter.thread->wake();
            ++woken;
        }
        return woken;
    }

    // Ends the wait of every waiter with result. Returns how many were woken.
    std::size_t wake_all(SceInt32 result = SCE_KERNEL_OK) {
        return wake_if([](Waiter &) { return true; }, result);
    }

    // Queues a waiter that the caller will block itself. Used by waits that need their own loop.
    void push(Waiter &waiter) {
        auto pos = waiters.end();
        if (by_priority) {
            // Lower value is higher priority, equal priorities keep FIFO order
            pos = std::find_if(waiters.begin(), waiters.end(), [&](const Waiter *w) { return w->priority > waiter.priority; });
            // A waiter whose place is the front goes right behind a head that keeps its place, so
            // it lands ahead of the waiters already behind that head, whatever their priority
            if (pos == waiters.begin() && pos != waiters.end() && keeps_place((*pos)->entry))
                ++pos;
        }
        waiters.insert(pos, &waiter);
    }

    // Takes a waiter off the queue if it is still on it.
    void remove(Waiter &waiter) {
        std::erase(waiters, &waiter);
    }

private:
    // Returns whether a waiter at the head stays there when another one would go in front of it, for
    // entries that can say so
    static bool keeps_place(const Entry &entry) {
        if constexpr (requires { entry.keeps_place(); })
            return entry.keeps_place();
        else
            return false;
    }

    bool by_priority = false;
    std::list<Waiter *> waiters;
};
