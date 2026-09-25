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

    WaitQueue() = default;
    explicit WaitQueue(SceUInt32 attr)
        : by_priority(attr & SCE_KERNEL_ATTR_TH_PRIO) {}

    // Blocks thread until a waker gives it a result, ready() holds, it is being deleted, or the deadline passes.
    // thread must be the guest thread making this HLE call. ready() is rechecked each time it is woken.
    WaitResult wait_until_ready(std::unique_lock<std::mutex> &lock, const ThreadStatePtr &thread, Entry entry, Deadline deadline, std::predicate<Waiter &> auto ready) {
        Waiter waiter{ .thread = thread, .entry = std::move(entry) };
        waiter.priority = waiter.thread->priority;
        push(waiter);
        while (true) {
            lock.unlock();
            const WaitResult r = waiter.thread->wait(deadline);
            lock.lock();
            // A waker's result wins, it already handed over what the waiter asked for
            if (waiter.result)
                break;
            if (!r) {
                remove(waiter);
                return r;
            }
            if (ready(waiter))
                break;
            if (*r == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
                remove(waiter);
                return r;
            }
        }
        remove(waiter);
        return waiter.result.value_or(SCE_KERNEL_OK);
    }

    // Blocks thread until a waker gives it a result, it is being deleted, or the deadline passes.
    // thread must be the guest thread making this HLE call.
    WaitResult wait(std::unique_lock<std::mutex> &lock, const ThreadStatePtr &thread, Entry entry, Deadline deadline) {
        return wait_until_ready(lock, thread, std::move(entry), deadline, [](Waiter &) { return false; });
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

    // Returns the first waiter accepted by pick, or null.
    Waiter *find_if(std::predicate<Waiter &> auto pick) {
        const auto it = std::find_if(waiters.begin(), waiters.end(), [&](Waiter *w) { return pick(*w); });
        return it == waiters.end() ? nullptr : *it;
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

    // Wakes a waiter so it rechecks its condition, leaving it queued.
    static void notify(Waiter &waiter) {
        waiter.thread->wake();
    }

    // Wakes every waiter so they recheck their condition, leaving them queued.
    void notify_all() {
        for (Waiter *waiter : waiters)
            notify(*waiter);
    }

    // Queues a waiter that the caller will block itself. Used by waits that need their own loop.
    void push(Waiter &waiter) {
        auto pos = waiters.end();
        if (by_priority) {
            // Lower value is higher priority, equal priorities keep FIFO order
            pos = std::find_if(waiters.begin(), waiters.end(), [&](const Waiter *w) { return w->priority > waiter.priority; });
        }
        waiters.insert(pos, &waiter);
    }

    // Takes a waiter off the queue if it is still on it.
    void remove(Waiter &waiter) {
        std::erase(waiters, &waiter);
    }

private:
    bool by_priority = false;
    std::list<Waiter *> waiters;
};
