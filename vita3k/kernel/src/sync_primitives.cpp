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
#include <util/log.h>

SyncPrimitive::SyncPrimitive(SceUInt32 attr, const char *name)
    : attr(attr)
    , name(name, strnlen(name, KERNELOBJECT_MAX_NAME_LENGTH)) {}

SceInt32 SyncPrimitive::check_name(const char *name, SceUInt32 attr) {
    if (!name)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if ((strlen(name) > KERNELOBJECT_MAX_NAME_LENGTH) && ((attr & 0x80) == 0x80))
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
    , pattern(init_pattern) {}

void SimpleEvent::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 SimpleEvent::wait_or_poll(const ThreadStatePtr &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool is_wait, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    if (result_pattern)
        *result_pattern = pattern;

    if (pattern & wait_pattern) {
        if (auto_reset)
            // all common bits are zeroed
            pattern &= ~wait_pattern;

        if (user_data)
            *user_data = last_user_data;

        return SCE_KERNEL_OK;
    } else if (is_wait) {
        const Deadline deadline = deadline_from(timeout);
        const WaitResult r = waiters.wait(guard, thread, { SCE_KERNEL_WAITTYPE_EVENT, uid }, { wait_pattern, result_pattern, user_data }, deadline, callbacks);
        writeback_timeout(timeout, deadline);
        const SceInt32 err = guest_result(r);
        if (err < 0) {
            // set it only if a timeout occurs
            // otherwise set in set_or_pulse
            if (user_data)
                *user_data = last_user_data;
            if (result_pattern)
                *result_pattern = pattern;
        }
        return err;
    } else {
        return SCE_KERNEL_ERROR_EVENT_COND;
    }
}

SceInt32 SimpleEvent::set_or_pulse(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    const SceUInt32 old_pattern = pattern;
    const SceUInt64 old_user_data = last_user_data;
    const SceUInt32 new_pattern = pattern | set_pattern;

    pattern = new_pattern;
    last_user_data = user_data;

    waiters.wake_if([&](auto &waiter) {
        const WaitEntry &wait = waiter.entry;
        if (!(pattern & wait.pattern))
            return false;

        if (wait.result_pattern)
            *wait.result_pattern = new_pattern;

        if (wait.user_data)
            *wait.user_data = last_user_data;

        if (auto_reset)
            // all common bit are zeroed
            pattern &= ~wait.pattern;

        return true;
    });

    if (!is_set) {
        pattern = old_pattern;
        last_user_data = old_user_data;
    }

    return SCE_KERNEL_OK;
}

SceInt32 SimpleEvent::clear(SceUInt32 clear_pattern) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

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

    is_pulse = type != 0;
    is_repeat = repeats != 0;
    event_interval = interval;

    if (is_started)
        schedule_event();

    return SCE_KERNEL_OK;
}

SceInt32 Timer::wait_or_poll(const ThreadStatePtr &thread, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool is_wait, bool callbacks) {
    if (timeout)
        LOG_WARN_ONCE("Ignoring timeout");

    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    if (result_pattern)
        *result_pattern = SCE_KERNEL_EVENT_TIMER;
    if (user_data)
        *user_data = 0;

    uint64_t current_time = get_current_time();
    auto set_next_event = [&]() {
        if (is_repeat) {
            // the event repeats every event_interval, go to the next one after current_time
            next_event += ((current_time - next_event - 1) / event_interval + 1) * event_interval;
        } else {
            next_event = std::numeric_limits<uint64_t>::max();
        }
    };

    if (next_event < current_time) {
        if (!is_pulse) {
            // we can reach pulse event only by waiting
            event_set = true;
        }

        set_next_event();
    }

    if (event_set) {
        if (attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET) {
            event_set = false;
        }

        return SCE_KERNEL_OK;
    } else if (is_wait) {
        WaitQueue<std::monostate>::Waiter waiter{ .thread = thread, .priority = thread->priority };
        waiters.push(waiter);

        while (true) {
            // A waker ended the wait, such as a delete
            if (waiter.result)
                return *waiter.result;
            // only the first waiter waits for the event, the others wait until they are first
            const bool is_first = waiters.front() == &waiter;
            Deadline deadline = Deadline::max();
            if (is_first && next_event != std::numeric_limits<uint64_t>::max()) {
                const uint64_t wait_time = next_event > current_time ? next_event - current_time : 0;
                deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(wait_time);
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
        }

        waiters.remove(waiter);

        event_set = !is_pulse && !(attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET);
        set_next_event();
        // notify the other waiting threads
        waiters.notify_all();

        return SCE_KERNEL_OK;
    } else {
        return SCE_KERNEL_ERROR_EVENT_COND;
    }
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

    is_started = true;
    time = get_current_time();

    if (event_interval != 0)
        schedule_event();

    return SCE_KERNEL_OK;
}

SceInt32 Timer::stop() {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    const bool was_stopped = !is_started;
    is_started = false;
    time = get_current_time();
    next_event = std::numeric_limits<uint64_t>::max();

    return static_cast<int>(was_stopped);
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

Mutex::Mutex(SceUInt32 attr, const char *name, int init_count, ThreadStatePtr thread, Ptr<SceKernelLwMutexWork> workarea)
    : SyncPrimitive(attr, name)
    , init_count(init_count)
    , workarea(workarea)
    , waiters(attr)
    , lock_count(init_count)
    , owner(init_count > 0 ? std::move(thread) : nullptr) {}

SceInt32 Mutex::check_create(const char *name, SceUInt32 attr, int init_count) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (init_count < 0)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (init_count > 1 && (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    return SCE_KERNEL_OK;
}

void Mutex::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    owner = nullptr;
}

SceInt32 Mutex::acquire(MemState &mem, const ThreadStatePtr &thread, int count, SceUInt32 *timeout, bool only_try, WaitTarget target, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;

    bool is_recursive = (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);

    // Already owned
    if (lock_count > 0) {
        // Owned by ourselves
        if (owner == thread) {
            if (is_recursive) {
                lock_count += count;
                if (lightweight())
                    workarea.get(mem)->lockCount += count;

                return SCE_KERNEL_OK;
            }
            if (lightweight())
                return SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;

            return SCE_KERNEL_ERROR_MUTEX_RECURSIVE;
        }
        // Owned by someone else

        // Don't sleep if only_try is set
        if (only_try) {
            if (lightweight())
                return SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;

            return SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN;
        }

        // Sleep thread!
        const Deadline deadline = deadline_from(timeout);
        const WaitResult r = waiters.wait(guard, thread, target, { count }, deadline, callbacks);
        writeback_timeout(timeout, deadline);

        // A deleted mutex has no owner, and its work area may be freed already
        if (lightweight() && owner == thread) {
            workarea.get(mem)->lockCount = lock_count;
            workarea.get(mem)->owner = thread->id;
        }

        return guest_result(r);
    }
    // Not owned
    // Take ownership!

    lock_count += count;
    owner = thread;

    if (lightweight()) {
        workarea.get(mem)->lockCount = lock_count;
        if (owner == thread) {
            workarea.get(mem)->owner = thread->id;
        }
    }

    return SCE_KERNEL_OK;
}

SceInt32 Mutex::release(const ThreadStatePtr &thread, int unlock_count) {
    const auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;

    if (thread == owner) {
        if (unlock_count > lock_count) {
            return SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF;
        }

        lock_count -= unlock_count;

        if (lock_count == 0) {
            owner = nullptr;

            if (auto *waiter = waiters.front()) {
                lock_count += waiter->entry.lock_count;
                owner = waiter->thread;
                waiters.wake(*waiter);
            }
        }
    }

    return SCE_KERNEL_OK;
}

SceInt32 Mutex::cancel(const ThreadStatePtr &thread, int new_count, SceUInt32 *num_wait_threads) {
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

void RWLock::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 RWLock::acquire(const ThreadStatePtr &thread, bool is_write, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    // if it is a read lock, it is always recursive
    bool is_recursive = !is_write || (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);

    // cases where we don't need to wait :
    if (state == RWLockState::Unlocked // the lock is unlocked
        || (!is_write && state == RWLockState::ReadLocked) // we want a read lock when the lock is readlocked
        || (is_recursive && owners.contains(thread))) { // the thread asking has already locked this lock

        auto it = owners.find(thread);
        if (it != owners.end()) {
            // increase the count
            it->second++;
        } else {
            owners.emplace(thread, 1);
        }

        state = is_write ? RWLockState::WriteLocked : RWLockState::ReadLocked;

        return SCE_KERNEL_OK;
    } else if (!is_recursive && owners.contains(thread)) {
        return SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE;
    } else {
        // we need to wait
        const Deadline deadline = deadline_from(timeout);
        const WaitResult r = waiters.wait(guard, thread, { SCE_KERNEL_WAITTYPE_RW_LOCK, uid }, { is_write }, deadline, callbacks);
        writeback_timeout(timeout, deadline);
        return guest_result(r);
    }
}

SceInt32 RWLock::release(const ThreadStatePtr &thread) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    auto it = owners.find(thread);
    if (it == owners.end()) {
        return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK;
    }

    // decrease the lock count
    it->second--;
    if (it->second == 0)
        owners.erase(thread);

    // if it is still locked
    if (!owners.empty())
        return SCE_KERNEL_OK;

    state = RWLockState::Unlocked;

    bool woke_writer = false;
    waiters.wake_if([&](auto &waiter) {
        const bool waiting_is_write = waiter.entry.is_write;

        if (woke_writer || (state == RWLockState::ReadLocked && waiting_is_write)) {
            // only awaken read threads
            return false;
        }

        owners.emplace(waiter.thread, 1);

        if (waiting_is_write) {
            state = RWLockState::WriteLocked;
            woke_writer = true;
        } else {
            state = RWLockState::ReadLocked;
        }
        return true;
    });

    return SCE_KERNEL_OK;
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

Semaphore::Semaphore(SceUInt32 attr, const char *name, int init_val, int max_val)
    : WithUidClass(attr, name)
    , init_val(init_val)
    , max(max_val)
    , waiters(attr)
    , val(init_val) {}

void Semaphore::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 Semaphore::wait(const ThreadStatePtr &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;

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

    if (val + count > max) {
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

    if (const SceInt32 error = associated_mutex->release(thread, 1))
        return error;

    const Deadline deadline = deadline_from(timeout);
    const WaitResult r = waiters.wait(guard, thread, { lightweight() ? SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL : SCE_KERNEL_WAITTYPE_COND_SIGNAL, uid }, {}, deadline, callbacks);
    writeback_timeout(timeout, deadline);
    if (!r || *r != SCE_KERNEL_OK)
        return guest_result(r);

    guard.unlock();
    // Taking the mutex back is still part of the condition variable wait
    const SceInt32 result = associated_mutex->acquire(mem, thread, 1, timeout, false, { lightweight() ? SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX : SCE_KERNEL_WAITTYPE_COND_MUTEX, uid }, callbacks);
    // Report a wait ended by the mutex with the mutex error codes
    if (result == SCE_KERNEL_ERROR_WAIT_DELETE)
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
        if (waiter) {
            waiters.wake(*waiter);
        } else {
            LOG_ERROR("Condition variable {}: target thread {} not found", uid, target.thread_id);
        }
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
    , flags(init_pattern) {}

void EventFlag::on_delete() {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 EventFlag::wait_or_poll(const ThreadStatePtr &thread, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits, SceUInt32 *timeout, bool is_wait, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    if ((attr & 0x1000) == 0 && !waiters.empty()) {
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

    if (condition) {
        if (wait_mode & SCE_EVENT_WAITCLEAR) {
            flags = 0;
        }

        if (wait_mode & SCE_EVENT_WAITCLEAR_PAT) {
            flags &= ~pattern;
        }

        return SCE_KERNEL_OK;
    } else if (is_wait) {
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
    } else {
        return SCE_KERNEL_ERROR_EVF_COND;
    }
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

    return 0;
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
    , receivers(attr)
    , data_buffer(buf_size) {}

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

SceSize MsgPipe::receive(const ThreadStatePtr &thread, SceUInt32 wait_mode, void *buf, SceSize size, SceUInt32 *timeout, bool callbacks) {
    const bool ASAP = !(wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);

    if (size > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    const auto copyOut = [&] {
        if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE) {
            return data_buffer.Peek(buf, size);
        } else {
            return data_buffer.Remove(buf, size);
        }
    };

    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;

    const auto can_receive = [&] {
        const std::size_t availableSize = data_buffer.Used();
        return (availableSize >= size) || (ASAP && availableSize >= 1);
    };

    if (!can_receive()) {
        if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT)
            return 0;

        // sleep until we can read, if ASAP we can read as low as 1 byte
        const WaitEntry entry{ .request_size = ASAP ? 1 : size };
        const WaitResult r = receivers.wait_until_ready(guard, thread, { SCE_KERNEL_WAITTYPE_MSG_PIPE, uid }, entry, deadline_from(timeout), callbacks, [&](auto &waiter) {
            waiter.entry.notified = false;
            return can_receive();
        });
        if (!r || *r != SCE_KERNEL_OK)
            return guest_result(r);
    }

    const SceSize copied_size = (SceSize)copyOut();
    wake_waiter(senders, data_buffer.Free());
    return copied_size;
}

// FIXME this should be SendVector!
SceSize MsgPipe::send(const ThreadStatePtr &thread, SceUInt32 wait_mode, const void *buf, SceSize size, SceUInt32 *timeout, bool callbacks) {
    const bool ASAP = !(wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);

    if (size > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;

    // If ASAP and there's at least 1 free byte, or FULL and there's enough space, copy and return directly.
    const auto can_send = [&] {
        const std::size_t freeSize = data_buffer.Free();
        return (freeSize >= size) || (ASAP && (freeSize >= 1));
    };

    if (!can_send()) {
        if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT)
            return 0;

        // sleep until there's more space, if ASAP we can insert as low as 1 byte
        const WaitEntry entry{ .request_size = ASAP ? 1 : size };
        const WaitResult r = senders.wait_until_ready(guard, thread, { SCE_KERNEL_WAITTYPE_MSG_PIPE, uid }, entry, deadline_from(timeout), callbacks, [&](auto &waiter) {
            waiter.entry.notified = false;
            return can_send();
        });
        if (!r || *r != SCE_KERNEL_OK)
            return guest_result(r);
    }

    const SceSize copied_size = (SceSize)data_buffer.Insert(buf, size);
    wake_waiter(receivers, data_buffer.Used());
    return copied_size;
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
