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

#include <algorithm>

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
    // The notify attribute is accepted and reported, nothing reads it
    if (attr & ~(SCE_KERNEL_EVENT_ATTR_AUTO_RESET | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE | SCE_KERNEL_ATTR_NOTIFY_CB_WAKEUP_ONLY))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (init_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
    return SCE_KERNEL_OK;
}

void SimpleEvent::on_delete(SceUID deleter) {
    // The waits for the delete event get it with the deleting thread's user data, then the others end
    fire(SCE_KERNEL_EVENT_DELETE, system_event_user_data(deleter), true);
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 SimpleEvent::try_take(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
    if (!(pattern & wait_pattern))
        return SCE_KERNEL_ERROR_EVENT_COND;

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
    if (const WaitResult r = run_pending_callbacks(guard, thread, callbacks); !r || *r != SCE_KERNEL_OK)
        return guest_result(r);

    // A wait for pattern 0 matches nothing, so it lasts until its timeout, a cancel or a delete
    if (const SceInt32 result = try_take(wait_pattern, result_pattern, user_data); result != SCE_KERNEL_ERROR_EVENT_COND)
        return result;

    const WaitResult r = wait_on(waiters, guard, thread, { SCE_KERNEL_WAITTYPE_EVENT, uid, callbacks }, { wait_pattern, result_pattern, user_data }, timeout);
    const SceInt32 err = guest_result(r);
    // A wait ended by a delete or a cancel gets the error event and no user data, and set_or_pulse
    // or on_delete writes what one that succeeds gets
    if (err < 0 && err != SCE_KERNEL_ERROR_WAIT_TIMEOUT && result_pattern)
        *result_pattern = SCE_KERNEL_EVENT_ERROR;
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

    fire(set_pattern, user_data, is_set);
    return SCE_KERNEL_OK;
}

SceInt32 SimpleEvent::set_system_event(SceUInt32 event, SceUInt64 user_data) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    fire(event, user_data, true);
    return SCE_KERNEL_OK;
}

void SimpleEvent::fire(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set) {
    // A set or pulse of no bits changes nothing, not even the user data
    if (set_pattern == 0)
        return;

    pattern |= set_pattern;
    last_user_data = user_data;
    serve();

    // A pulse clears its bits once the waiters had them, also the ones set before it, and keeps its
    // user data
    if (!is_set)
        pattern &= ~set_pattern;
}

void SimpleEvent::serve() {
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

SceInt32 SimpleEvent::cancel(SceUInt32 set_pattern, SceUInt64 user_data, SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (set_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;

    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    // Every bit is replaced, the create event too, and no waiter is left to get them
    pattern = set_pattern;
    last_user_data = user_data;
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

SceInt32 SimpleEvent::get_pattern(SceUInt32 &result) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    result = pattern;
    return SCE_KERNEL_OK;
}

SceInt32 SimpleEvent::info(SceKernelEventInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    info = {};
    info.eventId = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.eventPattern = pattern;
    info.userData = last_user_data;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
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

void Timer::on_delete(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

void Timer::schedule_event() {
    next_event = get_current_time() + event_interval;

    // the first waiter has to wait for the new event time
    wake_first();
}

void Timer::wake_first() {
    if (WaitQueue<WaitEntry>::Waiter *const first = waiters.front())
        first->thread->wake();
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
    if (const WaitResult r = run_pending_callbacks(guard, thread, callbacks); !r || *r != SCE_KERNEL_OK)
        return guest_result(r);

    uint64_t current_time = get_current_time();
    if (const SceInt32 result = try_take(result_pattern, user_data, current_time); result != SCE_KERNEL_ERROR_EVENT_COND)
        return result;

    const Deadline guest_deadline = deadline_from(timeout);
    WaitQueue<WaitEntry>::Waiter waiter{ .thread = thread, .priority = thread->get_priority(), .entry = { result_pattern } };
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
        WaitResult r = thread->wait({ SCE_KERNEL_WAITTYPE_EVENT, uid, callbacks }, deadline);
        if (r && *r == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            r = thread->run_callbacks();
        guard.lock();
        // A waker that ended the wait meanwhile, like an event that reached every waiter at once,
        // wins over the deadline and the thread's end
        if (waiter.result) {
            writeback_timeout(timeout, guest_deadline);
            return *waiter.result;
        }
        if (!r) {
            waiters.remove(waiter);
            wake_first();
            return guest_result(r);
        }

        current_time = get_current_time();
        if (waiters.front() == &waiter && (event_set || current_time > next_event))
            break;

        // The guest's timeout ran out before an event came
        if (std::chrono::steady_clock::now() >= guest_deadline) {
            waiters.remove(waiter);
            wake_first();
            writeback_timeout(timeout, guest_deadline);
            return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
        }
    }

    waiters.remove(waiter);
    writeback_timeout(timeout, guest_deadline);
    if (result_pattern)
        *result_pattern = SCE_KERNEL_EVENT_TIMER;

    // An event of a manual reset timer reaches every waiter at once, the events of an auto reset one
    // go to the waiters in turn
    if (!(attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)) {
        waiters.wake_if([](auto &w) {
            if (w.entry.result_pattern)
                *w.entry.result_pattern = SCE_KERNEL_EVENT_TIMER;
            return true;
        });
    }
    event_set = !is_pulse && !(attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET);
    advance_next_event(current_time);
    // the next waiter checks the event in turn
    wake_first();

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

SceInt32 Timer::get_time(uint64_t &time) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    time = count();
    return SCE_KERNEL_OK;
}

SceInt32 Timer::get_base(uint64_t &time) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    time = base;
    return SCE_KERNEL_OK;
}

SceInt32 Timer::event_info(SceKernelEventInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    // A set event that came is set, also before a wait or a poll took it
    const bool fired = event_set || (!is_pulse && next_event < get_current_time());
    info = {};
    info.eventId = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.eventPattern = SCE_KERNEL_EVENT_CREATE | (fired ? SCE_KERNEL_EVENT_TIMER : 0);
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
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
    , owner(init_count > 0 && thread ? thread->id : 0) {}

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

void Mutex::on_delete(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    owner = 0;
}

SceInt32 Mutex::try_take(MemState &mem, const ThreadStatePtr &thread, SceInt32 count) {
    bool is_recursive = (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);

    // Already owned
    if (lock_count > 0) {
        // Owned by ourselves
        if (owner == thread->id) {
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
    owner = thread->id;
    update_workarea(mem);

    return SCE_KERNEL_OK;
}

void Mutex::update_workarea(MemState &mem) {
    if (!lightweight())
        return;
    workarea.get(mem)->lockCount = lock_count;
    workarea.get(mem)->owner = owner;
}

SceInt32 Mutex::acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, bool callbacks) {
    return acquire(mem, thread, count, timeout, { lightweight() ? SCE_KERNEL_WAITTYPE_LW_MUTEX : SCE_KERNEL_WAITTYPE_MUTEX, uid, callbacks });
}

SceInt32 Mutex::acquire(MemState &mem, const ThreadStatePtr &thread, SceInt32 count, SceUInt32 *timeout, WaitTarget target) {
    auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    if (const SceInt32 error = check_count(count))
        return error;
    // The owner's own lock takes or fails at once, without callbacks
    if (lock_count == 0 || owner != thread->id) {
        if (const WaitResult r = run_pending_callbacks(guard, thread, target.callbacks); !r || *r != SCE_KERNEL_OK)
            return guest_result(r);
    }

    // Owned by someone else, wait until it is handed over
    if (lock_count > 0 && owner != thread->id) {
        const WaitResult r = wait_on(waiters, guard, thread, target, { count }, timeout);

        // A deleted mutex has no owner, and its work area may be freed already
        if (owner == thread->id)
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
    if (thread->id != owner)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED : SCE_KERNEL_ERROR_MUTEX_NOT_OWNED;
    if (unlock_count > lock_count)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF : SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF;

    lock_count -= unlock_count;
    if (lock_count == 0)
        owner = 0;
    serve();
    update_workarea(mem);

    return SCE_KERNEL_OK;
}

void Mutex::release_ended_owner(const ThreadState &thread) {
    const auto guard = lock();
    if (!guard || owner != thread.id)
        return;
    lock_count = 0;
    owner = 0;
    serve();
}

void Mutex::serve() {
    if (lock_count > 0)
        return;
    if (auto *waiter = waiters.front()) {
        lock_count = waiter->entry.lock_count;
        owner = waiter->thread->id;
        waiters.wake(*waiter);
    }
}

SceInt32 Mutex::cancel(const ThreadStatePtr &thread, SceInt32 new_count, SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;

    // Any negative count restores the lock count the mutex was created with
    if (new_count < 0)
        new_count = init_count;
    if (new_count > 1 && !(attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    lock_count = new_count;
    owner = new_count > 0 ? thread->id : 0;
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

SceInt32 LwMutex::info(SceKernelLwMutexInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;

    info = {};
    info.uid = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.pWork = workarea;
    info.initCount = init_count;
    info.currentCount = lock_count;
    info.currentOwnerId = owner;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
}

SceInt32 HeavyMutex::info(SceKernelMutexInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;

    info = {};
    info.mutexId = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.initCount = init_count;
    info.currentCount = lock_count;
    info.currentOwnerId = owner;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
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

void RWLock::on_delete(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 RWLock::try_take(const ThreadStatePtr &thread, bool is_write) {
    // The writer may take the write lock again only if the lock is recursive
    if (is_write && write_owner == thread->id) {
        if (!(attr & SCE_KERNEL_RW_LOCK_ATTR_RECURSIVE))
            return SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE;
        lock_count++;
        return SCE_KERNEL_OK;
    }

    // A waiting writer goes before new readers
    const bool writer_waits = waiters.find_if([](auto &waiter) { return waiter.entry.is_write; }) != nullptr;
    if (!is_write && writer_waits)
        return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;

    // Readers share the lock, but a reader and a writer don't, even in the same thread
    if (write_owner || (is_write && lock_count > 0))
        return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;

    lock_count++;
    if (is_write)
        write_owner = thread->id;
    return SCE_KERNEL_OK;
}

SceInt32 RWLock::acquire(const ThreadStatePtr &thread, bool is_write, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
    // The writer's own lock takes or fails at once, without callbacks
    if (write_owner != thread->id) {
        if (const WaitResult r = run_pending_callbacks(guard, thread, callbacks); !r || *r != SCE_KERNEL_OK)
            return guest_result(r);
    }

    if (const SceInt32 result = try_take(thread, is_write); result != SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK)
        return result;
    // The writer's own read lock fails at once, it could only wait for itself. A reader's write lock
    // waits, since readers aren't known one by one.
    if (!is_write && write_owner == thread->id)
        return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;

    // we need to wait
    const WaitResult r = wait_on(waiters, guard, thread, { SCE_KERNEL_WAITTYPE_RW_LOCK, uid, callbacks }, { is_write }, timeout);
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

    if (is_write) {
        // Only the writer may write unlock
        if (write_owner != thread->id)
            return SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED;
    } else {
        if (write_owner)
            return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK;
        if (lock_count == 0)
            return SCE_KERNEL_ERROR_RW_LOCK_UNLOCK_UDF;
    }

    if (--lock_count == 0)
        write_owner = 0;
    serve();
    return SCE_KERNEL_OK;
}

void RWLock::release_ended_writer(const ThreadState &thread) {
    const auto guard = lock();
    if (!guard)
        return;
    if (write_owner != thread.id)
        return;
    lock_count = 0;
    write_owner = 0;
    serve();
}

SceInt32 RWLock::info(SceKernelRWLockInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    info = {};
    info.rwLockId = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.lockCount = lock_count;
    info.writeOwnerId = write_owner;
    info.numReadWaitThreads = static_cast<SceUInt32>(waiters.count_if([](const auto &waiter) { return !waiter.entry.is_write; }));
    info.numWriteWaitThreads = static_cast<SceUInt32>(waiters.count_if([](const auto &waiter) { return waiter.entry.is_write; }));
    return SCE_KERNEL_OK;
}

void RWLock::serve() {
    if (write_owner)
        return;

    // Without a waiting writer every waiting reader gets in, also next to readers that hold the lock.
    // A waiting writer goes first, even before readers that waited longer, once nobody holds it.
    auto *const writer = waiters.find_if([](auto &waiter) { return waiter.entry.is_write; });
    if (!writer) {
        lock_count += static_cast<SceInt32>(waiters.wake_all());
    } else if (lock_count == 0) {
        write_owner = writer->thread->id;
        lock_count = 1;
        waiters.wake(*writer);
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
    const bool write_locked = flag & SCE_KERNEL_RW_LOCK_CANCEL_WITH_WRITE_LOCK;
    lock_count = write_locked ? 1 : 0;
    write_owner = write_locked ? thread->id : 0;

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

void Semaphore::on_delete(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 Semaphore::wait(const ThreadStatePtr &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    if (need_count <= 0 || need_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (const WaitResult r = run_pending_callbacks(guard, thread, callbacks); !r || *r != SCE_KERNEL_OK)
        return guest_result(r);

    if (try_take(thread, need_count))
        return SCE_KERNEL_OK;

    const WaitResult r = wait_on(waiters, guard, thread, { SCE_KERNEL_WAITTYPE_SEMAPHORE, uid, callbacks }, { need_count }, timeout);
    return guest_result(r);
}

SceInt32 Semaphore::poll(const ThreadStatePtr &thread, SceInt32 need_count) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    if (need_count <= 0 || need_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    return try_take(thread, need_count) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_SEMA_ZERO;
}

bool Semaphore::try_take(const ThreadStatePtr &thread, SceInt32 need_count) {
    // A waiter queued ahead goes first, even one the count doesn't cover yet
    if (val < need_count || !waiters.first_for(thread->get_priority()))
        return false;
    val -= need_count;
    return true;
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
    serve();

    return SCE_KERNEL_OK;
}

void Semaphore::serve() {
    while (auto *waiter = waiters.front()) {
        if (val < waiter->entry.need_count)
            break;
        val -= waiter->entry.need_count;
        waiters.wake(*waiter);
    }
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

SceInt32 Semaphore::info(SceKernelSemaInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;

    info = {};
    info.semaId = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.initCount = init_val;
    info.currentCount = val;
    info.maxCount = max;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
}

// **********************
// * Condition Variable *
// **********************

Condvar::Condvar(SceUInt32 attr, const char *name, MutexPtr associated_mutex)
    : SyncPrimitive(attr, name)
    , associated_mutex(std::move(associated_mutex))
    , waiters(attr) {}

void Condvar::on_delete(SceUID) {
    waiters.wake_all(lightweight() ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND : SCE_KERNEL_ERROR_WAIT_DELETE_COND);
}

SceInt32 Condvar::wait(MemState &mem, const ThreadStatePtr &thread, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID : SCE_KERNEL_ERROR_UNKNOWN_COND_ID;

    if (const SceInt32 error = associated_mutex->release(mem, thread, 1))
        return error;

    WaitResult r = run_pending_callbacks(guard, thread, callbacks);
    if (r && *r == SCE_KERNEL_OK)
        r = wait_on(waiters, guard, thread, { lightweight() ? SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL : SCE_KERNEL_WAITTYPE_COND_SIGNAL, uid, callbacks }, {}, timeout);
    // Deleted while the waiter ran its callbacks out of the queue, which ends it like the delete's wake
    if (r && *r == SCE_KERNEL_ERROR_WAIT_DELETE)
        return lightweight() ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND : SCE_KERNEL_ERROR_WAIT_DELETE_COND;
    // A timed out wait still takes the mutex back
    const bool timed_out = r && *r == SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    if (!r || (*r != SCE_KERNEL_OK && !timed_out))
        return guest_result(r);

    guard.unlock();
    // Taking the mutex back is still part of the condition variable wait, a wait on the mutex with no
    // timeout that leaves the time left as it was at the signal
    const SceInt32 result = associated_mutex->acquire(mem, thread, 1, nullptr, { lightweight() ? SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX : SCE_KERNEL_WAITTYPE_COND_MUTEX, associated_mutex->uid, callbacks });
    if (timed_out && result == SCE_KERNEL_OK)
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    // Report a wait ended by the mutex with the mutex error codes, also when the mutex was deleted
    // before the wait went back to it. On a Vita a lightweight mutex deleted then leaves the waiter
    // blocked instead, which isn't copied.
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

SceUInt32 Condvar::num_waiters() const {
    const auto guard = lock();
    return guard ? static_cast<SceUInt32>(waiters.size()) : 0;
}

SceInt32 HeavyCond::info(SceKernelCondInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_COND_ID;

    info = {};
    info.condId = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.mutexId = associated_mutex->uid;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
}

LwCond::LwCond(SceUInt32 attr, const char *name, MutexPtr associated_mutex, Ptr<SceKernelLwCondWork> workarea)
    : WithUidClass(attr, name, std::move(associated_mutex))
    , workarea(workarea) {}

SceInt32 LwCond::info(SceKernelLwCondInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID;

    info = {};
    info.uid = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.pWork = workarea;
    info.pLwMutex = associated_mutex->workarea;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
}

// **************
// * Event Flag *
// **************

EventFlag::EventFlag(SceUInt32 attr, const char *name, SceUInt32 init_pattern)
    : WithUidClass(attr, name)
    , init_pattern(init_pattern)
    , waiters(attr)
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

void EventFlag::on_delete(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    holder = 0;
    holds = 0;
}

void EventFlag::timed_out(const ThreadStatePtr &) {
    drop_hold();
}

void EventFlag::drop_hold() {
    if (holds > 0 && --holds == 0)
        holder = 0;
}

bool EventFlag::held_by_other(SceUID thread_id) const {
    return holds > 0 && holder != thread_id;
}

SceInt32 EventFlag::try_take(SceUID thread_id, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits) {
    if (held_by_other(thread_id))
        return SCE_KERNEL_ERROR_EVF_MULTI;

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
    if (held_by_other(thread->id))
        return SCE_KERNEL_ERROR_EVF_MULTI;
    if (const WaitResult r = run_pending_callbacks(guard, thread, callbacks); !r || *r != SCE_KERNEL_OK)
        return guest_result(r);

    if (const SceInt32 result = try_take(thread->id, pattern, wait_mode, out_bits); result != SCE_KERNEL_ERROR_EVF_COND)
        return result;

    if (!(attr & SCE_EVENT_WAITMULTIPLE)) {
        holder = thread->id;
        ++holds;
    }
    const WaitResult r = wait_on(waiters, guard, thread, { SCE_KERNEL_WAITTYPE_EVENTFLAG, uid, callbacks }, { wait_mode, pattern, out_bits }, timeout);
    const SceInt32 err = guest_result(r);
    if ((err == SCE_KERNEL_ERROR_WAIT_TIMEOUT || err == SCE_KERNEL_ERROR_WAIT_DELETE) && out_bits) {
        // set it only on a timeout or a delete
        // otherwise set in set or cancel
        *out_bits = flags;
    }

    return err;
}

SceInt32 EventFlag::poll(SceUID thread_id, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    if (const SceInt32 error = check_wait(pattern, wait_mode))
        return error;

    return try_take(thread_id, pattern, wait_mode, out_bits);
}

void EventFlag::release_ended_holder(const ThreadState &thread) {
    const auto guard = lock();
    if (!guard || holder != thread.id)
        return;
    holder = 0;
    holds = 0;
}

SceInt32 EventFlag::set(SceUInt32 pattern) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    flags |= pattern;
    serve();

    return SCE_KERNEL_OK;
}

void EventFlag::serve() {
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

        drop_hold();
        return true;
    });
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
        drop_hold();
        return true;
    };
    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_if(set_out_bits, SCE_KERNEL_ERROR_WAIT_CANCEL));

    flags = pattern;

    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

SceInt32 EventFlag::info(SceKernelEventFlagInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    info = {};
    info.evfId = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.initPattern = init_pattern;
    info.currentPattern = flags;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
}

// *************
// * Msg Pipe  *
// *************

MsgPipe::MsgPipe(SceUInt32 attr, const char *name, std::size_t buf_size)
    : WithUidClass(attr, name)
    , senders(attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_S ? WaitQueue<Sender>::Order::priority : WaitQueue<Sender>::Order::fifo)
    , receivers(attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_R ? WaitQueue<Receiver>::Order::priority : WaitQueue<Receiver>::Order::fifo)
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

void MsgPipe::on_delete(SceUID) {
    // Wake up every thread
    senders.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    receivers.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

// The size of all the pieces together
static uint64_t total_size(std::span<const MsgPipeVector> pieces) {
    uint64_t total = 0;
    for (const MsgPipeVector &piece : pieces)
        total += piece.size;
    return total;
}

MsgPipe::Transfer::Transfer(MemState &mem, std::span<const MsgPipeVector> pieces, SceSize *size, SceSize total, bool full)
    : pieces(pieces)
    , mem(&mem)
    , size(size)
    , total(total)
    , left(total)
    , full(full) {}

MsgPipe::Receiver::Receiver(MemState &mem, std::span<const MsgPipeVector> pieces, SceSize *size, SceSize total, bool full, bool peek)
    : Transfer(mem, pieces, size, total, full)
    , peek(peek) {}

bool MsgPipe::Transfer::done() const {
    return full ? left == 0 : left < total;
}

bool MsgPipe::Transfer::keeps_place() const {
    return left != total;
}

std::span<char> MsgPipe::Transfer::run(SceSize n) {
    // Steps over the pieces already moved, and empty ones
    while (offset == pieces[piece].size) {
        ++piece;
        offset = 0;
    }
    const MsgPipeVector &current = pieces[piece];
    return std::span{ current.base.cast<char>().get(*mem), current.size }.subspan(offset, std::min(n, current.size - offset));
}

void MsgPipe::Transfer::advance(SceSize n) {
    offset += n;
    left -= n;
}

void MsgPipe::Transfer::report() const {
    if (size)
        *size = total - left;
}

void MsgPipe::copy(Sender &sender, Receiver &receiver, SceSize n) {
    // A copy of the sender moves through its pieces, and a peek leaves the sender where it was
    Sender from = sender;
    while (n > 0) {
        const std::span<char> in = from.run(n);
        const std::span<char> out = receiver.run(static_cast<SceSize>(in.size()));
        std::ranges::copy(in.first(out.size()), out.begin());
        const SceSize moved = static_cast<SceSize>(out.size());
        from.advance(moved);
        receiver.advance(moved);
        n -= moved;
    }
    if (!receiver.peek)
        sender = from;
}

void MsgPipe::fill(Sender &sender) {
    // ASAP puts what fits, FULL all of its rest or nothing
    const SceSize room = static_cast<SceSize>(data_buffer.Free());
    if (sender.full && sender.left > room)
        return;
    SceSize n = std::min(sender.left, room);
    while (n > 0) {
        const std::span<char> in = sender.run(n);
        const SceSize moved = static_cast<SceSize>(data_buffer.Insert(in.data(), in.size()));
        sender.advance(moved);
        n -= moved;
    }
}

void MsgPipe::drain(Receiver &receiver) {
    const SceSize held = static_cast<SceSize>(data_buffer.Used());
    // A peek reads on from where it got to, and leaves the bytes in the pipe
    SceSize at = receiver.peek ? std::min(receiver.total - receiver.left, held) : 0;
    SceSize n = std::min(receiver.left, held - at);
    while (n > 0) {
        const std::span<char> out = receiver.run(n);
        const SceSize moved = static_cast<SceSize>(receiver.peek ? data_buffer.Peek(out.data(), out.size(), at) : data_buffer.Remove(out.data(), out.size()));
        at += moved;
        receiver.advance(moved);
        n -= moved;
    }
}

SceSize MsgPipe::available() const {
    SceSize bytes = static_cast<SceSize>(data_buffer.Used());
    for (const auto *sender : senders)
        bytes += sender->entry.left;
    return bytes;
}

bool MsgPipe::give(Sender &sender) {
    // A send reaches the first waiting receiver even with no bytes, which ends an ASAP receive
    while (auto *const receiver = receivers.front()) {
        Receiver &to = receiver->entry;
        copy(sender, to, std::min(to.left, sender.left));
        to.report();
        // A FULL receive that still wants bytes stays the first one
        if (to.full && to.left > 0)
            break;
        receivers.wake(*receiver);
        if (sender.left == 0)
            break;
    }
    fill(sender);
    sender.report();
    return sender.done();
}

bool MsgPipe::take(Receiver &receiver) {
    drain(receiver);
    // Each waiting sender gives its bytes to the receiver while it wants some, then puts its rest
    // into the pipe, up to the first sender that stays
    while (auto *const sender = senders.front()) {
        Sender &from = sender->entry;
        copy(from, receiver, std::min(receiver.left, from.left));
        // A peek frees nothing
        if (receiver.peek)
            break;
        fill(from);
        from.report();
        if (!from.done())
            break;
        senders.wake(*sender);
    }
    receiver.report();
    return receiver.done();
}

void MsgPipe::serve() {
    // The first receiver goes first, so the pipe's bytes go out before those of waiting senders
    while (true) {
        if (auto *const receiver = receivers.front(); receiver && take(receiver->entry))
            receivers.wake(*receiver);
        else if (auto *const sender = senders.front(); sender && give(sender->entry))
            senders.wake(*sender);
        else
            break;
    }
}

template <typename Side>
bool MsgPipe::goes_first(WaitQueue<Side> &side, int priority) {
    // Nothing goes ahead of a head that moved part of its bytes
    const auto *const head = side.front();
    return side.first_for(priority) && (!head || !head->entry.keeps_place());
}

SceInt32 MsgPipe::receive(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks) {
    if (!is_msg_pipe_mode(wait_mode))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    // DONT_WAIT ends like a try, and a wait with callbacks runs them after it
    if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT) {
        const SceInt32 result = try_receive(thread, mem, wait_mode, pieces, size);
        if (callbacks)
            thread->process_callbacks();
        return result;
    }
    const uint64_t total = total_size(pieces);
    if (total > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    Receiver receiver(mem, pieces, size, static_cast<SceSize>(total), wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE);
    // A receive of nothing ends at once, before any callback runs
    if (receiver.total == 0) {
        receiver.report();
        return SCE_KERNEL_OK;
    }
    if (const WaitResult r = run_pending_callbacks(guard, thread, callbacks); !r || *r != SCE_KERNEL_OK)
        return guest_result(r);
    // One behind the receivers already waiting takes nothing before it waits
    const bool done = goes_first(receivers, thread->get_priority()) && take(receiver);
    receiver.report();
    if (done)
        return SCE_KERNEL_OK;
    return guest_result(wait_on(receivers, guard, thread, { SCE_KERNEL_WAITTYPE_MSG_PIPE, uid, callbacks }, receiver, timeout));
}

SceInt32 MsgPipe::send(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks) {
    if (!is_msg_pipe_mode(wait_mode))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    // DONT_WAIT ends like a try, and a wait with callbacks runs them after it
    if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT) {
        const SceInt32 result = try_send(thread, mem, wait_mode, pieces, size);
        if (callbacks)
            thread->process_callbacks();
        return result;
    }
    const uint64_t total = total_size(pieces);
    if (total > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    Sender sender(mem, pieces, size, static_cast<SceSize>(total), wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);
    // A send of nothing ends at once, before any callback runs
    if (sender.total == 0) {
        sender.report();
        return SCE_KERNEL_OK;
    }
    if (const WaitResult r = run_pending_callbacks(guard, thread, callbacks); !r || *r != SCE_KERNEL_OK)
        return guest_result(r);
    // One behind the senders already waiting gives nothing before it waits
    const bool done = goes_first(senders, thread->get_priority()) && give(sender);
    sender.report();
    if (done)
        return SCE_KERNEL_OK;
    return guest_result(wait_on(senders, guard, thread, { SCE_KERNEL_WAITTYPE_MSG_PIPE, uid, callbacks }, sender, timeout));
}

SceInt32 MsgPipe::try_receive(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size) {
    if (!is_msg_pipe_mode(wait_mode))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    const uint64_t total = total_size(pieces);
    if (total > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    Receiver receiver(mem, pieces, size, static_cast<SceSize>(total), wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE);
    // Behind the receivers already waiting, or short of bytes for all of a FULL receive, it fails
    // without writing a size
    if (!goes_first(receivers, thread->get_priority()) || (receiver.full && receiver.total > available()))
        return SCE_KERNEL_ERROR_MSG_PIPE_EMPTY;
    return take(receiver) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_MSG_PIPE_EMPTY;
}

SceInt32 MsgPipe::try_send(const ThreadStatePtr &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size) {
    if (!is_msg_pipe_mode(wait_mode))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    const uint64_t total = total_size(pieces);
    if (total > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    Sender sender(mem, pieces, size, static_cast<SceSize>(total), wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);
    // Behind the senders already waiting, or short of room for all of a FULL send, it fails without
    // writing a size
    if (!goes_first(senders, thread->get_priority()) || (sender.full && sender.total > data_buffer.Free()))
        return SCE_KERNEL_ERROR_MSG_PIPE_FULL;
    return give(sender) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_MSG_PIPE_FULL;
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

SceInt32 MsgPipe::info(SceKernelMppInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;

    info = {};
    info.mppId = uid;
    strncpy(info.name, name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info.attr = attr;
    info.bufSize = static_cast<SceInt32>(data_buffer.Capacity());
    info.freeSize = static_cast<SceInt32>(data_buffer.Free());
    info.numSendWaitThreads = static_cast<SceInt32>(senders.size());
    info.numReceiveWaitThreads = static_cast<SceInt32>(receivers.size());
    return SCE_KERNEL_OK;
}
