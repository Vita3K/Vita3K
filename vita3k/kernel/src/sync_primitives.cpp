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

#include <cpu/functions.h>
#include <kernel/state.h>
#include <kernel/sync_primitives.h>

#include <kernel/types.h>
#include <util/align.h>
#include <util/log.h>

SyncPrimitive::SyncPrimitive(SceUInt32 attr, const char *name)
    : attr(attr)
    , name(name, strnlen(name, KERNELOBJECT_MAX_NAME_LENGTH)) {}

SceInt32 SyncPrimitive::check_name(const char *name, SceUInt32 attr) {
    if (!name)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if ((strlen(name) > KERNELOBJECT_MAX_NAME_LENGTH) && (attr & SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_UID_NAME_TOO_LONG;
    return SCE_KERNEL_OK;
}

// *****************
// * Simple events *
// *****************

SimpleEvent::SimpleEvent(SceUInt32 attr, const char *name, SceUInt32 init_pattern)
    : WithUidClass(attr, name)
    , auto_reset(attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
    , waiters(attr)
    , pattern(init_pattern | SCE_KERNEL_EVENT_CREATE) {}

SceInt32 SimpleEvent::check_create(const char *name, SceUInt32 attr, SceUInt32 init_pattern) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (attr & ~(SCE_KERNEL_EVENT_ATTR_AUTO_RESET | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (init_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
    return SCE_KERNEL_OK;
}

void SimpleEvent::on_delete() {
    // Waits for the delete event end with it, the others with an error
    waiters.wake_if([&](auto &waiter) {
        const WaitEntry &wait = waiter.entry;
        if (!(wait.pattern & SCE_KERNEL_EVENT_DELETE))
            return false;

        if (wait.result_pattern)
            *wait.result_pattern = (pattern | SCE_KERNEL_EVENT_DELETE) & wait.pattern;

        if (wait.user_data)
            *wait.user_data = last_user_data;

        return true;
    });
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 SimpleEvent::try_take(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
    if (!(pattern & wait_pattern)) {
        if (result_pattern)
            *result_pattern = 0;
        return SCE_KERNEL_ERROR_EVENT_COND;
    }

    if (result_pattern)
        *result_pattern = pattern & wait_pattern;

    if (auto_reset)
        // all common bits are zeroed
        pattern &= ~wait_pattern;

    if (user_data)
        *user_data = last_user_data;

    return SCE_KERNEL_OK;
}

SceInt32 SimpleEvent::wait(const ThreadStatePtr &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (wait_pattern == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    if (const SceInt32 result = try_take(wait_pattern, result_pattern, user_data); result != SCE_KERNEL_ERROR_EVENT_COND)
        return result;

    const Deadline deadline = deadline_from(timeout);
    const WaitResult r = waiters.wait(guard, thread, { SCE_KERNEL_WAITTYPE_EVENT, uid }, { wait_pattern, result_pattern, user_data }, deadline, callbacks);
    writeback_timeout(timeout, deadline);
    const SceInt32 err = guest_result(r);
    // A timed out wait keeps the 0 try_take wrote, one ended by a delete or a cancel gets the error
    // event, and set_or_pulse or on_delete writes the bits of one that succeeds
    if (err < 0 && err != SCE_KERNEL_ERROR_WAIT_TIMEOUT && result_pattern)
        *result_pattern = SCE_KERNEL_EVENT_ERROR;
    if (err < 0 && user_data)
        *user_data = last_user_data;
    return err;
}

SceInt32 SimpleEvent::poll(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (wait_pattern == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    return try_take(wait_pattern, result_pattern, user_data);
}

SceInt32 SimpleEvent::set_or_pulse(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (set_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;

    const SceUInt32 old_pattern = pattern;
    const SceUInt64 old_user_data = last_user_data;
    pattern |= set_pattern;
    last_user_data = user_data;

    waiters.wake_if([&](auto &waiter) {
        const WaitEntry &wait = waiter.entry;
        if (!(pattern & wait.pattern))
            return false;

        if (wait.result_pattern)
            *wait.result_pattern = pattern & wait.pattern;

        if (wait.user_data)
            *wait.user_data = last_user_data;

        if (auto_reset)
            // all common bit are zeroed
            pattern &= ~wait.pattern;

        return true;
    });

    // A pulse clears its own bits, the others go back to what they were
    if (!is_set) {
        pattern = old_pattern & ~set_pattern;
        last_user_data = old_user_data;
    }

    return SCE_KERNEL_OK;
}

SceInt32 SimpleEvent::clear(SceUInt32 clear_pattern) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (clear_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;

    pattern &= clear_pattern;

    return SCE_KERNEL_OK;
}

SceInt32 SimpleEvent::cancel(SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

// *********
// * Timer *
// *********

inline uint64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

Timer::Timer(SceUInt32 attr, const char *name)
    : WithUidClass(attr, name)
    , waiters(attr) {}

void Timer::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

void Timer::schedule_event() {
    next_event = get_current_time() + event_interval;

    // the first waiter has to wait for the new event time
    waiters.notify_all();
}

SceInt32 Timer::set_event(SceUID type, SceKernelSysClock interval, SceInt32 repeats) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    if (type != SCE_KERNEL_TIMER_TYPE_SET_EVENT && type != SCE_KERNEL_TIMER_TYPE_PULSE_EVENT)
        return SCE_KERNEL_ERROR_ILLEGAL_TYPE;

    is_pulse = type == SCE_KERNEL_TIMER_TYPE_PULSE_EVENT;
    is_repeat = repeats != 0;
    event_interval = interval;

    if (is_started)
        schedule_event();

    return SCE_KERNEL_OK;
}

void Timer::advance_next_event(uint64_t current_time) {
    if (is_repeat) {
        // the event repeats every event_interval, go to the next one after current_time
        next_event += ((current_time - next_event - 1) / event_interval + 1) * event_interval;
    } else {
        next_event = std::numeric_limits<uint64_t>::max();
    }
}

SceInt32 Timer::try_take(SceUInt32 *result_pattern, SceUInt64 *user_data, uint64_t current_time) {
    // A take that fails gets no bits, like a simple event poll
    if (result_pattern)
        *result_pattern = 0;
    if (user_data)
        *user_data = 0;

    if (next_event < current_time) {
        if (!is_pulse) {
            // we can reach pulse event only by waiting
            event_set = true;
        }

        advance_next_event(current_time);
    }

    if (!event_set)
        return SCE_KERNEL_ERROR_EVENT_COND;

    if (result_pattern)
        *result_pattern = SCE_KERNEL_EVENT_TIMER;
    if (attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
        event_set = false;

    return SCE_KERNEL_OK;
}

SceInt32 Timer::wait(const ThreadStatePtr &thread, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    uint64_t current_time = get_current_time();
    if (const SceInt32 result = try_take(result_pattern, user_data, current_time); result != SCE_KERNEL_ERROR_EVENT_COND)
        return result;

    const Deadline guest_deadline = deadline_from(timeout);
    WaitQueue<std::monostate>::Waiter waiter{ .thread = thread, .priority = thread->get_priority() };
    waiters.push(waiter);

    while (true) {
        // A waker ended the wait, such as a delete
        if (waiter.result) {
            writeback_timeout(timeout, guest_deadline);
            return *waiter.result;
        }
        // only the first waiter waits for the event, the others wait until they are first
        const bool is_first = waiters.front() == &waiter;
        Deadline deadline = guest_deadline;
        if (is_first && next_event != std::numeric_limits<uint64_t>::max()) {
            const uint64_t wait_time = next_event > current_time ? next_event - current_time : 0;
            deadline = std::min(deadline, std::chrono::steady_clock::now() + std::chrono::microseconds(wait_time));
        }

        guard.unlock();
        const WaitResult r = thread->wait({ SCE_KERNEL_WAITTYPE_EVENT, uid }, deadline, callbacks);
        guard.lock();
        if (!r) {
            waiters.remove(waiter);
            waiters.notify_all();
            return guest_result(r);
        }

        current_time = get_current_time();
        if (waiters.front() == &waiter && (event_set || current_time > next_event))
            break;

        // The guest's timeout ran out before an event came
        if (std::chrono::steady_clock::now() >= guest_deadline) {
            waiters.remove(waiter);
            waiters.notify_all();
            writeback_timeout(timeout, guest_deadline);
            return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
        }
    }

    waiters.remove(waiter);
    writeback_timeout(timeout, guest_deadline);
    if (result_pattern)
        *result_pattern = SCE_KERNEL_EVENT_TIMER;

    event_set = !is_pulse && !(attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET);
    advance_next_event(current_time);
    // notify the other waiting threads
    waiters.notify_all();

    return SCE_KERNEL_OK;
}

SceInt32 Timer::poll(SceUInt32 *result_pattern, SceUInt64 *user_data) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    return try_take(result_pattern, user_data, get_current_time());
}

SceInt32 Timer::clear() {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    event_set = false;
    return SCE_KERNEL_OK;
}

SceInt32 Timer::start() {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    if (is_started)
        return 1;

    // The count goes on from where it stopped
    is_started = true;
    base = get_current_time() - stopped_count;

    if (event_interval != 0)
        schedule_event();

    return SCE_KERNEL_OK;
}

SceInt32 Timer::stop() {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    if (!is_started)
        return 0;

    stopped_count = count();
    is_started = false;
    next_event = std::numeric_limits<uint64_t>::max();

    return 1;
}

std::expected<uint64_t, SceInt32> Timer::set_time(uint64_t new_time) {
    const auto guard = lock();
    if (!guard)
        return std::unexpected(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    const uint64_t old_count = count();
    if (is_started)
        base = get_current_time() - new_time;
    else
        stopped_count = new_time;
    return old_count;
}

uint64_t Timer::count() const {
    return is_started ? get_current_time() - base : stopped_count;
}

SceInt32 Timer::cancel(SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    // The event set with sceKernelSetTimerEvent is canceled too
    is_repeat = false;
    is_pulse = false;
    event_interval = 0;
    next_event = std::numeric_limits<uint64_t>::max();
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

// *********
// * Mutex *
// *********

Mutex::Mutex(SceUInt32 attr, const char *name, SceInt32 init_count, ThreadStatePtr thread, Ptr<SceKernelLwMutexWork> workarea)
    : SyncPrimitive(attr, name)
    , init_count(init_count)
    , workarea(workarea)
    , waiters(attr)
    , lock_count(init_count)
    , owner(init_count > 0 ? std::move(thread) : nullptr) {}

SceInt32 Mutex::check_create(const char *name, SceUInt32 attr, SceInt32 init_count, bool kernel) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (!kernel && attr & ~(SCE_KERNEL_MUTEX_ATTR_RECURSIVE | SCE_KERNEL_MUTEX_ATTR_CEILING | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (init_count < 0)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (init_count > 1 && !(attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    return SCE_KERNEL_OK;
}

SceInt32 Mutex::check_count(SceInt32 count) const {
    if (count <= 0 || (count > 1 && !(attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE)))
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    return SCE_KERNEL_OK;
}

void Mutex::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    owner = nullptr;
}

SceInt32 Mutex::try_take(MemState &mem, const ThreadStatePtr &thread, SceInt32 count) {
    bool is_recursive = (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);

    // Already owned
    if (lock_count > 0) {
        // Owned by ourselves
        if (owner == thread) {
            if (is_recursive) {
                if (lightweight()) {
                    // A lightweight mutex doesn't check for overflow, its count just wraps
                    lock_count = static_cast<SceInt32>(static_cast<SceUInt32>(lock_count) + static_cast<SceUInt32>(count));
                    workarea.get(mem)->lockCount = lock_count;
                    return SCE_KERNEL_OK;
                }
                if (count > std::numeric_limits<SceInt32>::max() - lock_count)
                    return SCE_KERNEL_ERROR_MUTEX_LOCK_OVF;
                lock_count += count;
                return SCE_KERNEL_OK;
            }
            if (lightweight())
                return SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;

            return SCE_KERNEL_ERROR_MUTEX_RECURSIVE;
        }
        // Owned by someone else
        if (lightweight())
            return SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;

        return SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN;
    }
    // Not owned
    // Take ownership!

    lock_count += count;
    owner = thread;
    update_workarea(mem);

    return SCE_KERNEL_OK;
}

void Mutex::update_workarea(MemState &mem) {
    if (!lightweight())
        return;
    workarea.get(mem)->lockCount = lock_count;
    workarea.get(mem)->owner = owner ? owner->id : 0;
}

SceInt32 Mutex::acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, bool callbacks) {
    return acquire(mem, thread, count, timeout, { lightweight() ? SCE_KERNEL_WAITTYPE_LW_MUTEX : SCE_KERNEL_WAITTYPE_MUTEX, uid }, callbacks);
}

SceInt32 Mutex::acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, WaitTarget target, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    if (const SceInt32 error = check_count(count))
        return error;

    // Owned by someone else, wait until it is handed over
    if (lock_count > 0 && owner != thread) {
        const Deadline deadline = deadline_from(timeout);
        const WaitResult r = waiters.wait(guard, thread, target, { count }, deadline, callbacks);
        writeback_timeout(timeout, deadline);

        // A deleted mutex has no owner, and its work area may be freed already
        if (owner == thread)
            update_workarea(mem);

        return guest_result(r);
    }

    return try_take(mem, thread, count);
}

SceInt32 Mutex::try_acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count) {
    const auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    if (const SceInt32 error = check_count(count))
        return error;

    return try_take(mem, thread, count);
}

SceInt32 Mutex::release(MemState &mem, const ThreadStatePtr &thread, SceInt32 unlock_count) {
    const auto guard = lock();
    // Deleted meanwhile, which a lightweight mutex reports as not owned
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;

    if (const SceInt32 error = check_count(unlock_count))
        return error;
    if (thread != owner)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED : SCE_KERNEL_ERROR_MUTEX_NOT_OWNED;
    if (unlock_count > lock_count)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF : SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF;

    lock_count -= unlock_count;

    if (lock_count == 0)
        hand_over();
    update_workarea(mem);

    return SCE_KERNEL_OK;
}

void Mutex::release_ended_owner() {
    lock_count = 0;
    hand_over();
}

void Mutex::hand_over() {
    owner = nullptr;
    if (auto *waiter = waiters.front()) {
        lock_count += waiter->entry.lock_count;
        owner = waiter->thread;
        waiters.wake(*waiter);
    }
}

SceInt32 Mutex::cancel(const ThreadStatePtr &thread, SceInt32 new_count, SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;

    // -1 restores the lock count the mutex was created with
    if (new_count == -1)
        new_count = init_count;
    if (new_count < 0 || (new_count > 1 && !(attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE)))
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    lock_count = new_count;
    owner = new_count > 0 ? thread : nullptr;
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

// **************
// * RWLock *
// **************

RWLock::RWLock(SceUInt32 attr, const char *name)
    : WithUidClass(attr, name)
    , waiters(attr) {}

SceInt32 RWLock::check_create(const char *name, SceUInt32 attr) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (attr & ~(SCE_KERNEL_RW_LOCK_ATTR_RECURSIVE | SCE_KERNEL_RW_LOCK_ATTR_CEILING | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    return SCE_KERNEL_OK;
}

void RWLock::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 RWLock::try_take(const ThreadStatePtr &thread, bool is_write) {
    // The writer may take the write lock again only if the lock is recursive
    if (is_write && state == RWLockState::WriteLocked && owners.contains(thread)) {
        if (!(attr & SCE_KERNEL_RW_LOCK_ATTR_RECURSIVE))
            return SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE;
        owners[thread]++;
        return SCE_KERNEL_OK;
    }

    // A waiting writer goes before new readers
    const bool writer_waits = waiters.find_if([](auto &waiter) { return waiter.entry.is_write; }) != nullptr;
    if (!is_write && writer_waits)
        return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;

    // Readers share the lock, but a reader and a writer don't, even in the same thread
    if (state == RWLockState::Unlocked || (!is_write && state == RWLockState::ReadLocked)) {
        owners[thread]++;
        state = is_write ? RWLockState::WriteLocked : RWLockState::ReadLocked;
        return SCE_KERNEL_OK;
    }

    return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;
}

SceInt32 RWLock::acquire(const ThreadStatePtr &thread, bool is_write, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    if (const SceInt32 result = try_take(thread, is_write); result != SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK)
        return result;

    // we need to wait
    const Deadline deadline = deadline_from(timeout);
    const WaitResult r = waiters.wait(guard, thread, { SCE_KERNEL_WAITTYPE_RW_LOCK, uid }, { is_write }, deadline, callbacks);
    writeback_timeout(timeout, deadline);
    return guest_result(r);
}

SceInt32 RWLock::try_acquire(const ThreadStatePtr &thread, bool is_write) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    return try_take(thread, is_write);
}

SceInt32 RWLock::try_acquire_read(const ThreadStatePtr &thread) {
    return try_acquire(thread, false);
}

SceInt32 RWLock::try_acquire_write(const ThreadStatePtr &thread) {
    return try_acquire(thread, true);
}

SceInt32 RWLock::acquire_read(const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks) {
    return acquire(thread, false, timeout, callbacks);
}

SceInt32 RWLock::acquire_write(const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks) {
    return acquire(thread, true, timeout, callbacks);
}

SceInt32 RWLock::release_read(const ThreadStatePtr &thread) {
    return release(thread, false);
}

SceInt32 RWLock::release_write(const ThreadStatePtr &thread) {
    return release(thread, true);
}

SceInt32 RWLock::release(const ThreadStatePtr &thread, bool is_write) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    // Only the writer may write unlock, and only a reader may read unlock
    if (!is_write && state == RWLockState::Unlocked)
        return SCE_KERNEL_ERROR_RW_LOCK_UNLOCK_UDF;
    auto it = owners.find(thread);
    const RWLockState held = is_write ? RWLockState::WriteLocked : RWLockState::ReadLocked;
    if (state != held || it == owners.end())
        return is_write ? SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED : SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK;

    // decrease the lock count
    it->second--;
    if (it->second == 0)
        owners.erase(thread);

    // if it is still locked
    if (!owners.empty())
        return SCE_KERNEL_OK;

    hand_over();
    return SCE_KERNEL_OK;
}

void RWLock::release_ended_writer(const ThreadState &thread) {
    // A write lock has a single owner, whatever its count
    if (state != RWLockState::WriteLocked || owners.empty() || owners.begin()->first.get() != &thread)
        return;
    owners.clear();
    hand_over();
}

void RWLock::hand_over() {
    state = RWLockState::Unlocked;

    // The first waiting writer goes first, even before readers that waited longer. Without one,
    // every waiting reader gets the lock at once.
    if (auto *writer = waiters.find_if([](auto &waiter) { return waiter.entry.is_write; })) {
        owners.emplace(writer->thread, 1);
        state = RWLockState::WriteLocked;
        waiters.wake(*writer);
    } else if (waiters.wake_if([&](auto &waiter) {
                   owners.emplace(waiter.thread, 1);
                   return true;
               })) {
        state = RWLockState::ReadLocked;
    }
}

SceInt32 RWLock::cancel(const ThreadStatePtr &thread, SceUInt32 *num_read_wait_threads, SceUInt32 *num_write_wait_threads, SceInt32 flag) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    const auto is_writer = [](auto &waiter) { return waiter.entry.is_write; };
    const auto nb_writers = static_cast<SceUInt32>(waiters.wake_if(is_writer, SCE_KERNEL_ERROR_WAIT_CANCEL));
    const auto nb_readers = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));

    // The lock is reset, and only the calling thread may hold it afterwards
    owners.clear();
    if (flag & SCE_KERNEL_RW_LOCK_CANCEL_WITH_WRITE_LOCK) {
        owners.emplace(thread, 1);
        state = RWLockState::WriteLocked;
    } else {
        state = RWLockState::Unlocked;
    }

    if (num_read_wait_threads)
        *num_read_wait_threads = nb_readers;
    if (num_write_wait_threads)
        *num_write_wait_threads = nb_writers;

    return SCE_KERNEL_OK;
}

// **************
// * Semaphore *
// **************

Semaphore::Semaphore(SceUInt32 attr, const char *name, SceInt32 init_val, SceInt32 max_val)
    : WithUidClass(attr, name)
    , init_val(init_val)
    , max(max_val)
    , waiters(attr)
    , val(init_val) {}

SceInt32 Semaphore::check_create(const char *name, SceUInt32 attr, SceInt32 init_val, SceInt32 max_val) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (attr & ~(SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (init_val < 0 || max_val <= 0 || init_val > max_val)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    return SCE_KERNEL_OK;
}

void Semaphore::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 Semaphore::wait(const ThreadStatePtr &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    if (need_count <= 0 || need_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    if (val < need_count) {
        const Deadline deadline = deadline_from(timeout);
        const WaitResult r = waiters.wait(guard, thread, { SCE_KERNEL_WAITTYPE_SEMAPHORE, uid }, { need_count }, deadline, callbacks);
        writeback_timeout(timeout, deadline);
        return guest_result(r);
    } else {
        val -= need_count;
    }

    return SCE_KERNEL_OK;
}

SceInt32 Semaphore::poll(SceInt32 need_count) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    if (need_count <= 0 || need_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    if (val < need_count) {
        return SCE_KERNEL_ERROR_SEMA_ZERO;
    }
    val -= need_count;
    return SCE_KERNEL_OK;
}

SceInt32 Semaphore::signal(SceInt32 count) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;

    if (count < 0)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (count > max - val) {
        return SCE_KERNEL_ERROR_SEMA_OVF;
    }
    val += count;

    while (auto *waiter = waiters.front()) {
        const auto waiting_signal_count = waiter->entry.need_count;

        if (val < waiting_signal_count)
            break;

        val -= waiting_signal_count;
        waiters.wake(*waiter);
    }

    return SCE_KERNEL_OK;
}

SceInt32 Semaphore::cancel(SceInt32 set_count, SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;

    if (set_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    if (set_count < 0) {
        val = init_val;
    } else {
        val = set_count;
    }
    if (num_wait_threads)
        *num_wait_threads = nb_threads;
    return SCE_KERNEL_OK;
}

// **********************
// * Condition Variable *
// **********************

Condvar::Condvar(SceUInt32 attr, const char *name, MutexPtr associated_mutex)
    : SyncPrimitive(attr, name)
    , associated_mutex(std::move(associated_mutex))
    , waiters(attr) {}

void Condvar::on_delete() {
    waiters.wake_all(lightweight() ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND : SCE_KERNEL_ERROR_WAIT_DELETE_COND);
}

SceInt32 Condvar::wait(MemState &mem, const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID : SCE_KERNEL_ERROR_UNKNOWN_COND_ID;

    if (const SceInt32 error = associated_mutex->release(mem, thread, 1))
        return error;

    const Deadline deadline = deadline_from(timeout);
    const WaitResult r = waiters.wait(guard, thread, { lightweight() ? SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL : SCE_KERNEL_WAITTYPE_COND_SIGNAL, uid }, {}, deadline, callbacks);
    writeback_timeout(timeout, deadline);
    // A timed out wait still takes the mutex back, as long as that takes
    const bool timed_out = r && *r == SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    if (!r || (*r != SCE_KERNEL_OK && !timed_out))
        return guest_result(r);

    guard.unlock();
    // Taking the mutex back is still part of the condition variable wait
    const SceInt32 result = associated_mutex->acquire(mem, thread, 1, timed_out ? nullptr : timeout, { lightweight() ? SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX : SCE_KERNEL_WAITTYPE_COND_MUTEX, uid }, callbacks);
    if (timed_out && result == SCE_KERNEL_OK)
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    // Report a wait ended by the mutex with the mutex error codes, also when the mutex was deleted
    // before the wait went back to it
    if (result == SCE_KERNEL_ERROR_WAIT_DELETE || result == SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID || result == SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID)
        return lightweight() ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX : SCE_KERNEL_ERROR_WAIT_DELETE_MUTEX;
    // only heavy mutexes can be canceled
    if (result == SCE_KERNEL_ERROR_WAIT_CANCEL)
        return SCE_KERNEL_ERROR_WAIT_CANCEL_MUTEX;
    return result;
}

SceInt32 Condvar::signal(SignalTarget target) {
    const auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID : SCE_KERNEL_ERROR_UNKNOWN_COND_ID;

    if (target.type == SignalTarget::Type::Specific) {
        // Search for specified waiting thread
        auto *waiter = waiters.find_if([&](auto &w) { return w.thread->id == target.thread_id; });
        if (!waiter)
            return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
        waiters.wake(*waiter);
    } else if (target.type == SignalTarget::Type::Any) {
        // Wakes the first waiter in queue order
        if (auto *waiter = waiters.front())
            waiters.wake(*waiter);
    } else {
        waiters.wake_all();
    }

    return SCE_KERNEL_OK;
}

// **************
// * Event Flag *
// **************

EventFlag::EventFlag(SceUInt32 attr, const char *name, SceUInt32 init_pattern)
    : WithUidClass(attr, name)
    , waiters(attr)
    , init_pattern(init_pattern)
    , flags(init_pattern) {}

SceInt32 EventFlag::check_create(const char *name, SceUInt32 attr) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (attr & ~(SCE_EVENT_WAITMULTIPLE | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    return SCE_KERNEL_OK;
}

SceInt32 EventFlag::check_wait(SceUInt32 pattern, SceUInt32 wait_mode) {
    if (pattern == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    if (wait_mode & ~(SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR | SCE_EVENT_WAITCLEAR_PAT))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    // Only one way of clearing the bits after the wait
    if ((wait_mode & SCE_EVENT_WAITCLEAR) && (wait_mode & SCE_EVENT_WAITCLEAR_PAT))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    return SCE_KERNEL_OK;
}

void EventFlag::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 EventFlag::try_take(SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits) {
    if (!(attr & SCE_EVENT_WAITMULTIPLE) && !waiters.empty()) {
        return SCE_KERNEL_ERROR_EVF_MULTI;
    }

    bool condition;
    if (wait_mode & SCE_EVENT_WAITOR) {
        condition = flags & pattern;
    } else {
        condition = (flags & pattern) == pattern;
    }

    if (out_bits) {
        *out_bits = flags;
    }

    if (!condition)
        return SCE_KERNEL_ERROR_EVF_COND;

    if (wait_mode & SCE_EVENT_WAITCLEAR) {
        flags = 0;
    }

    if (wait_mode & SCE_EVENT_WAITCLEAR_PAT) {
        flags &= ~pattern;
    }

    return SCE_KERNEL_OK;
}

SceInt32 EventFlag::wait(const ThreadStatePtr &thread, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    if (const SceInt32 error = check_wait(pattern, wait_mode))
        return error;

    if (const SceInt32 result = try_take(pattern, wait_mode, out_bits); result != SCE_KERNEL_ERROR_EVF_COND)
        return result;

    const Deadline deadline = deadline_from(timeout);
    const WaitResult r = waiters.wait(guard, thread, { SCE_KERNEL_WAITTYPE_EVENTFLAG, uid }, { wait_mode, pattern, out_bits }, deadline, callbacks);
    writeback_timeout(timeout, deadline);
    const SceInt32 err = guest_result(r);
    if ((err == SCE_KERNEL_ERROR_WAIT_TIMEOUT || err == SCE_KERNEL_ERROR_WAIT_DELETE) && out_bits) {
        // set it only on a timeout or a delete
        // otherwise set in set or cancel
        *out_bits = flags;
    }

    return err;
}

SceInt32 EventFlag::poll(SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    if (const SceInt32 error = check_wait(pattern, wait_mode))
        return error;

    return try_take(pattern, wait_mode, out_bits);
}

SceInt32 EventFlag::set(SceUInt32 pattern) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    flags |= pattern;

    waiters.wake_if([&](auto &waiter) {
        const WaitEntry &wait = waiter.entry;
        const SceUInt32 waiting_flags = wait.pattern;

        bool condition;
        if (wait.wait_mode & SCE_EVENT_WAITOR) {
            condition = flags & waiting_flags;
        } else {
            condition = (flags & waiting_flags) == waiting_flags;
        }

        if (!condition)
            return false;

        if (wait.out_bits) {
            *wait.out_bits = flags;
        }

        if (wait.wait_mode & SCE_EVENT_WAITCLEAR) {
            flags = 0;
        }

        if (wait.wait_mode & SCE_EVENT_WAITCLEAR_PAT) {
            flags &= ~waiting_flags;
        }

        return true;
    });

    return SCE_KERNEL_OK;
}

SceInt32 EventFlag::clear(SceUInt32 pattern) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    flags &= pattern;

    return SCE_KERNEL_OK;
}

SceInt32 EventFlag::cancel(SceUInt32 pattern, SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    const auto set_out_bits = [&](auto &waiter) {
        if (waiter.entry.out_bits)
            *waiter.entry.out_bits = pattern;
        return true;
    };
    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_if(set_out_bits, SCE_KERNEL_ERROR_WAIT_CANCEL));

    flags = pattern;

    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

// *************
// * Msg Pipe  *
// *************

MsgPipe::MsgPipe(SceUInt32 attr, const char *name, std::size_t buf_size)
    : WithUidClass(attr, name)
    , senders(attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_S ? WaitQueue<WaitEntry>::Order::priority : WaitQueue<WaitEntry>::Order::fifo)
    , receivers(attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_R ? WaitQueue<WaitEntry>::Order::priority : WaitQueue<WaitEntry>::Order::fifo)
    // A pipe's buffer is a whole number of 4 KiB pages
    , data_buffer(align(buf_size, 0x1000)) {}

SceInt32 MsgPipe::check_create(const char *name, SceUInt32 type, SceUInt32 attr, SceSize buf_size) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (type != SCE_KERNEL_MSG_PIPE_TYPE_USER_MAIN)
        return SCE_KERNEL_ERROR_ILLEGAL_TYPE;
    if (attr & ~(SCE_KERNEL_MSG_PIPE_ATTR_TH_MASK | SCE_KERNEL_ATTR_OPENABLE | SCE_KERNEL_EVENT_ATTR_AUTO_RESET | SCE_KERNEL_ATTR_NOTIFY_CB_WAKEUP_ONLY))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (buf_size == 0)
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    return SCE_KERNEL_OK;
}

// Returns whether a send or receive takes this wait mode
static bool is_msg_pipe_mode(SceUInt32 wait_mode) {
    return !(wait_mode & ~(SCE_KERNEL_MSG_PIPE_MODE_FULL | SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT | SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE));
}

void MsgPipe::on_delete() {
    // Wake up every thread
    senders.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    receivers.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

void MsgPipe::wake_waiter(WaitQueue<WaitEntry> &waiters, std::size_t available) {
    auto *waiter = waiters.find_if([&](auto &w) {
        return !w.entry.notified && w.entry.request_size <= available;
    });
    if (waiter) {
        waiter->entry.notified = true;
        WaitQueue<WaitEntry>::notify(*waiter);
    }
}

// The size of all the pieces together
static uint64_t total_size(std::span<const MsgPipeVector> pieces) {
    uint64_t total = 0;
    for (const MsgPipeVector &piece : pieces)
        total += piece.size;
    return total;
}

std::expected<SceSize, SceInt32> MsgPipe::receive(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceUInt32 *timeout, bool callbacks) {
    const bool ASAP = !(wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);

    if (!is_msg_pipe_mode(wait_mode))
        return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_MODE);
    if (total_size(pieces) > data_buffer.Capacity())
        return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_SIZE);
    const SceSize size = static_cast<SceSize>(total_size(pieces));

    // Fills the pieces in order with what the pipe holds
    const auto copyOut = [&] {
        std::size_t copied = 0;
        for (const MsgPipeVector &piece : pieces) {
            if (piece.size == 0)
                continue;
            void *const out = piece.base.get(mem);
            const std::size_t piece_copied = (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE) ? data_buffer.Peek(out, piece.size, copied) : data_buffer.Remove(out, piece.size);
            copied += piece_copied;
            if (piece_copied < piece.size)
                break;
        }
        return copied;
    };

    auto guard = lock();
    if (!guard)
        return std::unexpected(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);

    // An ASAP receive needs at least 1 byte to move, so one of 0 bytes never ends
    const auto can_receive = [&] {
        const std::size_t availableSize = data_buffer.Used();
        return ASAP ? size >= 1 && availableSize >= 1 : availableSize >= size;
    };

    if (!can_receive()) {
        if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT)
            return std::unexpected(SCE_KERNEL_ERROR_MSG_PIPE_EMPTY);

        // sleep until we can read, if ASAP we can read as low as 1 byte
        const WaitEntry entry{ .request_size = ASAP ? 1 : size };
        const Deadline deadline = deadline_from(timeout);
        const WaitResult r = receivers.wait_until_ready(guard, thread, { SCE_KERNEL_WAITTYPE_MSG_PIPE, uid }, entry, deadline, callbacks, [&](auto &waiter) {
            waiter.entry.notified = false;
            return can_receive();
        });
        writeback_timeout(timeout, deadline);
        // The thread is exiting, which the guest never sees
        if (!r)
            return 0;
        if (*r != SCE_KERNEL_OK)
            return std::unexpected(*r);
    }

    const SceSize copied_size = (SceSize)copyOut();
    wake_waiter(senders, data_buffer.Free());
    return copied_size;
}

std::expected<SceSize, SceInt32> MsgPipe::send(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceUInt32 *timeout, bool callbacks) {
    const bool ASAP = !(wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);

    if (!is_msg_pipe_mode(wait_mode))
        return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_MODE);
    if (total_size(pieces) > data_buffer.Capacity())
        return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_SIZE);
    const SceSize size = static_cast<SceSize>(total_size(pieces));

    auto guard = lock();
    if (!guard)
        return std::unexpected(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);

    // An ASAP send needs at least 1 byte to move, so one of 0 bytes never ends
    const auto can_send = [&] {
        const std::size_t freeSize = data_buffer.Free();
        return ASAP ? size >= 1 && freeSize >= 1 : freeSize >= size;
    };

    if (!can_send()) {
        if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT)
            return std::unexpected(SCE_KERNEL_ERROR_MSG_PIPE_FULL);

        // sleep until there's more space, if ASAP we can insert as low as 1 byte
        const WaitEntry entry{ .request_size = ASAP ? 1 : size };
        const Deadline deadline = deadline_from(timeout);
        const WaitResult r = senders.wait_until_ready(guard, thread, { SCE_KERNEL_WAITTYPE_MSG_PIPE, uid }, entry, deadline, callbacks, [&](auto &waiter) {
            waiter.entry.notified = false;
            return can_send();
        });
        writeback_timeout(timeout, deadline);
        // The thread is exiting, which the guest never sees
        if (!r)
            return 0;
        if (*r != SCE_KERNEL_OK)
            return std::unexpected(*r);
    }

    // Copies the pieces in order into the pipe, as far as it has room
    std::size_t copied = 0;
    for (const MsgPipeVector &piece : pieces) {
        if (piece.size == 0)
            continue;
        const std::size_t piece_copied = data_buffer.Insert(piece.base.get(mem), piece.size);
        copied += piece_copied;
        if (piece_copied < piece.size)
            break;
    }
    wake_waiter(receivers, data_buffer.Used());
    return static_cast<SceSize>(copied);
}

SceInt32 MsgPipe::cancel(SceUInt32 *num_send_wait_threads, SceUInt32 *num_receive_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;

    const auto nb_senders = static_cast<SceUInt32>(senders.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    const auto nb_receivers = static_cast<SceUInt32>(receivers.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    data_buffer.Clear();

    if (num_send_wait_threads)
        *num_send_wait_threads = nb_senders;
    if (num_receive_wait_threads)
        *num_receive_wait_threads = nb_receivers;

    return SCE_KERNEL_OK;
}
