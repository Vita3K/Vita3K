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

#include <kernel/state.h>
#include <kernel/sync_primitives.h>

#include <kernel/types.h>
#include <util/align.h>
#include <util/log.h>

#include <algorithm>
#include <cassert>

// Whether a queued message pipe transfer moved part of its bytes, which keeps it at the head of its
// queue
static bool keeps_place(const WaitRequest &request) {
    if (request.sender)
        return request.sender->keeps_place();
    if (request.receiver)
        return request.receiver->keeps_place();
    return false;
}

ThreadState *WaitQueue::front() {
    return threads.empty() ? nullptr : &threads.front();
}

bool WaitQueue::first_for(int priority) const {
    if (threads.empty())
        return true;
    // Lower value is higher priority, and equal priorities keep FIFO order
    return by_priority && threads.front().queued_priority > priority;
}

void WaitQueue::wake(ThreadState &thread, SceInt32 result) {
    thread.ender_result = result;
    remove(thread);
    thread.wake();
}

std::size_t WaitQueue::wake_all(SceInt32 result) {
    return wake_if([](ThreadState &) { return true; }, result);
}

void WaitQueue::push(ThreadState &thread, const WaitRequest &request) {
    assert(!thread.linked());
    thread.request = request;
    thread.ender_result.reset();
    thread.queued_priority = thread.get_priority();
    auto pos = threads.end();
    if (by_priority) {
        // Lower value is higher priority, equal priorities keep FIFO order
        pos = threads.begin();
        while (pos != threads.end() && pos->queued_priority <= thread.queued_priority)
            ++pos;
        // A thread whose place is the front goes right behind a head that keeps its place, so it
        // lands ahead of the threads already behind that head, whatever their priority
        if (pos == threads.begin() && pos != threads.end() && keeps_place(pos->wait_request()))
            ++pos;
    }
    threads.insert(pos, thread);
}

void WaitQueue::remove(ThreadState &thread) {
    threads.remove(thread);
}

Waitable::Waitable(SceUInt32 attr, const char *name)
    : KernelObject(name)
    , attr(attr)
    , waiters(attr) {}

Waitable::Waitable(SceUInt32 attr, WaitQueue::Order order, const char *name)
    : KernelObject(name)
    , attr(attr)
    , waiters(order) {}

void Waitable::on_delete(SceUID deleter) {
    const std::lock_guard<std::mutex> guard(mutex);
    assert(!deleted);
    deleted = true;
    end_waits(deleter);
}

SceInt32 Waitable::run_wait_callbacks(std::unique_lock<std::mutex> &lock, ThreadState &thread, bool callbacks) {
    if (!callbacks || !thread.has_pending_callbacks())
        return SCE_KERNEL_OK;
    // The lock is released while they run, so the object may be deleted meanwhile
    lock.unlock();
    SceInt32 r = thread.run_callbacks();
    lock.lock();
    if (r == SCE_KERNEL_OK && deleted)
        r = SCE_KERNEL_ERROR_WAIT_DELETE;
    return r;
}

SceInt32 Waitable::wait(WaitQueue &queue, std::unique_lock<std::mutex> &lock, ThreadState &thread, SceUInt32 type, const WaitRequest &request, Deadline deadline, SceUInt32 *timeout) {
    const bool callbacks = type & SCE_KERNEL_WAITTYPE_CB_BIT;
    if (std::chrono::steady_clock::now() >= deadline) {
        writeback_timeout(timeout, deadline);
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    }
    // A callback already pending leaves the timeout as it was
    if (callbacks && thread.has_pending_callbacks())
        return SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
    queue.push(thread, request);
    const WaitTarget target{ type, uid };
    SceInt32 result;
    // Ignore stray wakeups, like one left over from an earlier wait
    do {
        lock.unlock();
        result = thread.wait(target, deadline);
        lock.lock();
    } while (!thread.ender_result && result == SCE_KERNEL_OK);
    writeback_timeout(timeout, deadline);
    // A waker's result wins, it already handed over what the waiter waits for, unless the thread is
    // ending. Read once, since a wait in the callbacks below may reuse it.
    if (const std::optional<SceInt32> ended = thread.ender_result; ended && result != SCE_KERNEL_ERROR_THREAD_STOPPED) {
        // The callback that ended the sleep runs before the waker's result is returned
        if (result == SCE_KERNEL_ERROR_NOTIFY_CALLBACK && run_wait_callbacks(lock, thread, callbacks) == SCE_KERNEL_ERROR_THREAD_STOPPED)
            return SCE_KERNEL_ERROR_THREAD_STOPPED;
        return *ended;
    }
    queue.remove(thread);
    wait_end(thread, result);
    serve();
    return result;
}

SceInt32 Waitable::finish_wait(std::unique_lock<std::mutex> &lock, ThreadState &thread, bool callbacks, SceInt32 result) {
    if (result == SCE_KERNEL_ERROR_THREAD_STOPPED)
        return result;
    // Only the thread exiting changes the result
    if (run_wait_callbacks(lock, thread, callbacks) == SCE_KERNEL_ERROR_THREAD_STOPPED)
        return SCE_KERNEL_ERROR_THREAD_STOPPED;
    return result;
}

SceInt32 Waitable::check_name(const char *name, SceUInt32 attr) {
    if (!name)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if ((strlen(name) > SCE_UID_NAMELEN) && (attr & SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_UID_NAME_TOO_LONG;
    return SCE_KERNEL_OK;
}

// *****************
// * Simple events *
// *****************

Event::Event(SceUInt32 attr, const char *name, SceUInt32 init_pattern, SceUInt64 init_user_data)
    : Waitable(attr, name)
    , auto_reset(attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
    , pattern(init_pattern | SCE_KERNEL_EVENT_CREATE)
    , last_user_data(init_user_data) {}

// The create event comes with the creating thread's user data, like the other system events
SimpleEvent::SimpleEvent(SceUInt32 attr, const char *name, SceUInt32 init_pattern, SceUID creator)
    : WithUidClass(attr, name, init_pattern, system_event_user_data(creator)) {}

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

void Event::end_waits(SceUID deleter) {
    // The waits for the delete event get it with the deleting thread's user data, then the others end
    fire(SCE_KERNEL_EVENT_DELETE, system_event_user_data(deleter), true);
    notify_callbacks(SCE_KERNEL_EVENT_DELETE);
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 Event::register_callback(KernelState &kernel, CallbackRef callback, SceUID notify_id) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    // After the registrations of the same callback
    auto pos = registrations.begin();
    while (pos != registrations.end() && pos->callback->uid <= callback->uid)
        ++pos;
    registrations.insert(pos, std::make_unique<CallbackRegistration>(kernel, std::move(callback), notify_id));
    return SCE_KERNEL_OK;
}

SceInt32 Event::unregister_callback(SceUID callback_id) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    for (CallbackRegistration &registration : registrations) {
        if (registration.callback->uid == callback_id) {
            registrations.remove(registration);
            return SCE_KERNEL_OK;
        }
    }
    return SCE_KERNEL_ERROR_CALLBACK_NOT_REGISTERED;
}

SceInt32 Event::unregister_callbacks() {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    while (!registrations.empty())
        registrations.remove(*registrations.begin());
    return SCE_KERNEL_OK;
}

void Event::notify_callbacks(SceInt32 arg) {
    for (auto it = registrations.begin(); it != registrations.end();) {
        CallbackRegistration &registration = *it;
        ++it;
        Callback &callback = *registration.callback;
        // A callback whose owner is gone, or that was closed, is dropped
        const ThreadStateRef owner = registration.kernel.get_thread(callback.thread_id);
        if (!owner || !owner->notify_callback(callback, arg, registration.notify_id))
            registrations.remove(registration);
    }
}

SceInt32 Event::try_take(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
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

SceInt32 SimpleEvent::wait(ThreadState &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    // A wait for pattern 0 matches nothing, so it lasts until its timeout, a cancel or a delete
    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_EVENT, callbacks);
    const Deadline deadline = deadline_from(timeout);
    SceInt32 result = SCE_KERNEL_OK;
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return wait_result(r, result_pattern);
        // If the bits are taken right after a callback notify, the wait returns NOTIFY_CALLBACK
        if (try_take(wait_pattern, result_pattern, user_data) == SCE_KERNEL_OK)
            return wait_result(result, result_pattern);
        result = Waitable::wait(waiters, guard, thread, type, { .pattern = wait_pattern, .out_pattern = result_pattern, .out_user_data = user_data }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return wait_result(finish_wait(guard, thread, callbacks, result), result_pattern);
    }
}

SceInt32 Event::wait_result(SceInt32 err, SceUInt32 *result_pattern) {
    // A wait ended by a delete or a cancel gets the error event and no user data. If the wait got
    // the bits, they are already written, also when it returns NOTIFY_CALLBACK.
    if (err < 0 && err != SCE_KERNEL_ERROR_WAIT_TIMEOUT && err != SCE_KERNEL_ERROR_NOTIFY_CALLBACK && err != SCE_KERNEL_ERROR_THREAD_STOPPED && result_pattern)
        *result_pattern = SCE_KERNEL_EVENT_ERROR;
    return err;
}

SceInt32 Event::poll(SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    catch_up();
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

SceInt32 SimpleEvent::set_or_pulse_and_notify(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set, SceInt32 notify_arg) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (set_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
    if (set_pattern == 0)
        return SCE_KERNEL_OK;

    // The waiters get the bits before the callbacks are notified
    fire(set_pattern, user_data, is_set);
    notify_callbacks(notify_arg);
    return SCE_KERNEL_OK;
}

SceInt32 SimpleEvent::set_system_event(SceUInt32 event, SceUInt64 user_data) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    fire(event, user_data, true);
    // The callbacks get the event's bit as the argument
    notify_callbacks(event);
    return SCE_KERNEL_OK;
}

void Event::fire(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set) {
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

void Event::serve() {
    waiters.wake_if([&](auto &waiter) {
        const WaitRequest &wait = waiter.wait_request();
        if (!(pattern & wait.pattern))
            return false;

        if (wait.out_pattern)
            *wait.out_pattern = pattern & wait.pattern;

        if (wait.out_user_data)
            *wait.out_user_data = last_user_data;

        if (auto_reset)
            // all common bit are zeroed
            pattern &= ~wait.pattern;

        return true;
    });
}

SceInt32 Event::clear(SceUInt32 clear_pattern) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    catch_up();
    if (clear_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;

    pattern &= clear_pattern;

    return SCE_KERNEL_OK;
}

SceInt32 Event::cancel(SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    catch_up();

    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

SceInt32 Event::cancel(SceUInt32 set_pattern, SceUInt64 user_data, SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    catch_up();
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

SceInt32 Event::get_pattern(SceUInt32 &result) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    catch_up();
    result = pattern;
    return SCE_KERNEL_OK;
}

SceInt32 Event::info(SceKernelEventInfo &info) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    catch_up();

    info = {};
    info.eventId = uid;
    std::ranges::copy(name, info.name);
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

Timer::Timer(SceUInt32 attr, const char *name, SceUID creator)
    : WithUidClass(attr, name, 0, system_event_user_data(creator)) {}

// A timer event is due at least this long after it was queued or after the event before it
constexpr uint64_t TIMER_EVENT_FLOOR = 200;

void Timer::catch_up() {
    const uint64_t now = get_current_time();
    if (next_event > now)
        return;
    fire(SCE_KERNEL_EVENT_TIMER, 0, !is_pulse);
    notify_callbacks(0);
    // A one shot event leaves the queue but stays armed, so a start or a set time can queue it again
    if (!is_repeat) {
        next_event = std::numeric_limits<uint64_t>::max();
        return;
    }
    // A repeating event is due again an interval after it was due. With nobody waiting, the events
    // it missed meanwhile are skipped, so its callbacks get one notification for all of them.
    const uint64_t interval = std::max(event_interval, TIMER_EVENT_FLOOR);
    uint64_t next = next_event + interval;
    if (waiters.empty() && next <= now)
        next += ((now - next) / interval + 1) * interval;
    next_event = std::max(next, now + TIMER_EVENT_FLOOR);
    schedule = next_event - base;
}

void Timer::queue_event(uint64_t deadline) {
    next_event = std::max(deadline, get_current_time() + TIMER_EVENT_FLOOR);
    // The first waiter sleeps until the old event time, so wake it to see the new one
    if (ThreadState *const first = waiters.front())
        first->wake();
}

Deadline Timer::event_deadline() const {
    if (next_event == std::numeric_limits<uint64_t>::max())
        return Deadline::max();
    const uint64_t now = get_current_time();
    return std::chrono::steady_clock::now() + std::chrono::microseconds(next_event > now ? next_event - now : 0);
}

SceInt32 Timer::set_event(SceUID type, SceKernelSysClock interval, SceInt32 repeats) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    if (type != SCE_KERNEL_TIMER_TYPE_SET_EVENT && type != SCE_KERNEL_TIMER_TYPE_PULSE_EVENT)
        return SCE_KERNEL_ERROR_ILLEGAL_TYPE;
    catch_up();

    is_pulse = type == SCE_KERNEL_TIMER_TYPE_PULSE_EVENT;
    is_repeat = repeats != 0;
    event_interval = interval;
    armed = true;
    schedule = count() + interval;
    // A stopped timer queues the event when it starts
    if (is_started)
        queue_event(get_current_time() + interval);
    else
        next_event = std::numeric_limits<uint64_t>::max();

    return SCE_KERNEL_OK;
}

SceInt32 Timer::wait(ThreadState &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    const Deadline deadline = deadline_from(timeout);
    SceInt32 result = SCE_KERNEL_OK;
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return wait_result(r, result_pattern);
        // Events that are already due fire before the take, so the waiter can't get an old pulse
        catch_up();
        if (try_take(wait_pattern, result_pattern, user_data) == SCE_KERNEL_OK)
            return wait_result(result, result_pattern);
        result = sleep(guard, thread, callbacks, { .pattern = wait_pattern, .out_pattern = result_pattern, .out_user_data = user_data }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return wait_result(finish_wait(guard, thread, callbacks, result), result_pattern);
    }
}

SceInt32 Timer::sleep(std::unique_lock<std::mutex> &lock, ThreadState &thread, bool callbacks, const WaitRequest &request, Deadline deadline, SceUInt32 *timeout) {
    if (std::chrono::steady_clock::now() >= deadline) {
        writeback_timeout(timeout, deadline);
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    }
    // A callback already pending leaves the timeout as it was
    if (callbacks && thread.has_pending_callbacks())
        return SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
    waiters.push(thread, request);
    const WaitTarget target{ wait_type(SCE_KERNEL_WAITTYPE_EVENT, callbacks), uid };
    SceInt32 result;
    while (true) {
        const Deadline wake = waiters.front() == &thread ? std::min(deadline, event_deadline()) : deadline;
        lock.unlock();
        result = thread.wait(target, wake);
        lock.lock();
        if (thread.ender_result || result == SCE_KERNEL_ERROR_NOTIFY_CALLBACK || result == SCE_KERNEL_ERROR_THREAD_STOPPED)
            break;
        // Woken because the next event is due, the schedule changed, or for no reason. The waiter
        // stays queued.
        catch_up();
        if (thread.ender_result || std::chrono::steady_clock::now() >= deadline) {
            result = SCE_KERNEL_ERROR_WAIT_TIMEOUT;
            break;
        }
    }
    writeback_timeout(timeout, deadline);
    if (thread.ender_result && result != SCE_KERNEL_ERROR_THREAD_STOPPED)
        return *thread.ender_result;
    waiters.remove(thread);
    serve();
    return result;
}

void Timer::serve() {
    Event::serve();
    // The first waiter may be a new one, which now has to wake for the next event
    if (ThreadState *const first = waiters.front())
        first->wake();
}

SceInt32 Timer::start() {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    if (is_started)
        return 1;
    catch_up();

    // The count goes on from where it stopped, and an armed event is due after the time it has left,
    // at once if it already fired
    is_started = true;
    const uint64_t now = get_current_time();
    base = now - stopped_count;
    if (armed)
        queue_event(now + (schedule > stopped_count ? schedule - stopped_count : 0));

    return SCE_KERNEL_OK;
}

SceInt32 Timer::stop() {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    if (!is_started)
        return 0;
    catch_up();

    stopped_count = count();
    is_started = false;
    next_event = std::numeric_limits<uint64_t>::max();

    return 1;
}

std::expected<uint64_t, SceInt32> Timer::set_time(uint64_t new_time) {
    const auto guard = lock();
    if (!guard)
        return std::unexpected(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);
    catch_up();

    const uint64_t old_count = count();
    if (is_started) {
        const uint64_t now = get_current_time();
        base = now - new_time;
        // If the new time is before the armed event's time, the event is due when the count reaches
        // it again. Otherwise it stays due when it was.
        if (armed && new_time < schedule)
            queue_event(now + (schedule - new_time));
    } else {
        stopped_count = new_time;
    }
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
    time = is_started ? base : 0;
    return SCE_KERNEL_OK;
}

SceInt32 Timer::remaining_time(uint64_t &time) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    catch_up();
    if (!armed)
        return SCE_KERNEL_ERROR_EVENT_NOT_SET;
    time = schedule - count();
    return SCE_KERNEL_OK;
}

SceInt32 Timer::timer_info(SceKernelTimerInfo &info) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    catch_up();

    info = {};
    info.timerId = uid;
    std::ranges::copy(name, info.name);
    info.attr = attr;
    info.active = is_started;
    // The base of a running timer, the count of a stopped one
    info.baseTime = is_started ? base : stopped_count;
    info.currentTime = count();
    info.schedule = armed ? schedule : 0;
    info.interval = event_interval;
    info.type = is_pulse ? SCE_KERNEL_TIMER_TYPE_PULSE_EVENT : SCE_KERNEL_TIMER_TYPE_SET_EVENT;
    info.repeat = is_repeat;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
}

SceInt32 Timer::cancel(SceUInt32 *num_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    catch_up();

    const auto nb_threads = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    // The event set with sceKernelSetTimerEvent is canceled too, but the timer keeps running with
    // the event's interval and type
    armed = false;
    is_repeat = false;
    next_event = std::numeric_limits<uint64_t>::max();
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

// *********
// * Mutex *
// *********

Mutex::Mutex(SceUInt32 attr, const char *name, SceInt32 init_count, ThreadStateRef thread, int ceiling, Ptr<SceKernelLwMutexWork> workarea)
    : Waitable(attr, name)
    , init_count(init_count)
    , workarea(workarea)
    , ceiling(ceiling)
    , lock_count(init_count)
    , owner(init_count > 0 && thread ? thread->id : 0) {}

HeavyMutex::HeavyMutex(SceUInt32 attr, const char *name, SceInt32 init_count, ThreadStateRef thread, int ceiling)
    : WithUidClass(attr, name, init_count, thread, ceiling) {
    if (owner)
        set_owner(*thread);
}

SceInt32 Mutex::check_create(const char *name, SceUInt32 attr, SceInt32 init_count, const SceKernelMutexOptParam *opt, bool kernel) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (!kernel && attr & ~(SCE_KERNEL_MUTEX_ATTR_RECURSIVE | SCE_KERNEL_MUTEX_ATTR_CEILING | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    // A smaller option struct is still read whole
    if (opt && opt->size > sizeof(*opt))
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    if (!opt && (attr & SCE_KERNEL_MUTEX_ATTR_CEILING))
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
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

void Mutex::end_waits(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    clear_owner();
}

void Mutex::set_owner(ThreadState &thread) {
    owner = thread.id;
    if (lightweight() || owner_thread == &thread)
        return;
    if (owner_thread)
        owner_thread->remove_held(*this);
    owner_thread = &thread;
    thread.add_held(MutexRef(this));
}

void Mutex::clear_owner() {
    owner = 0;
    if (!owner_thread)
        return;
    owner_thread->remove_held(*this);
    owner_thread = nullptr;
}

SceInt32 Mutex::try_take(MemState &mem, ThreadState &thread, SceInt32 count) {
    bool is_recursive = (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);

    // Already owned
    if (owner) {
        // Owned by ourselves
        if (owner == thread.id) {
            if (is_recursive) {
                if (lightweight()) {
                    // A lightweight count wraps past the maximum, and a lock that would leave it
                    // below 0 fails
                    const int64_t sum = static_cast<int64_t>(lock_count) + count;
                    if (sum < 0)
                        return SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
                    lock_count = static_cast<SceInt32>(static_cast<SceUInt32>(sum));
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
    set_owner(thread);
    update_workarea(mem);

    return SCE_KERNEL_OK;
}

void Mutex::update_workarea(MemState &mem) {
    if (!lightweight())
        return;
    workarea.get(mem)->lockCount = lock_count;
    workarea.get(mem)->owner = owner;
}

SceInt32 Mutex::acquire(MemState &mem, ThreadState &thread, SceInt32 count, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    if (const SceInt32 error = check_count(count))
        return error;
    // The owner's own lock takes or fails at once, without callbacks
    if (owner == thread.id)
        return try_take(mem, thread, count);

    const SceUInt32 type = wait_type(lightweight() ? SCE_KERNEL_WAITTYPE_LW_MUTEX : SCE_KERNEL_WAITTYPE_MUTEX, callbacks);
    const Deadline deadline = deadline_from(timeout);
    SceInt32 r;
    while (true) {
        r = run_wait_callbacks(guard, thread, callbacks);
        if (r != SCE_KERNEL_OK)
            break;
        // A heavy mutex also needs no waiter ahead of this thread. A mutex acquired by this
        // thread's callbacks still blocks this wait.
        if (!owner && (lightweight() || waiters.first_for(thread.get_priority()))) {
            r = try_take(mem, thread, count);
            break;
        }
        r = Waitable::wait(waiters, guard, thread, type, { .count = count }, deadline, timeout);
        if (r != SCE_KERNEL_ERROR_NOTIFY_CALLBACK) {
            r = finish_wait(guard, thread, callbacks, r);
            break;
        }
    }
    // A deleted mutex has no owner, and its work area may be freed already
    if (owner == thread.id)
        update_workarea(mem);
    return r;
}

SceInt32 Mutex::relock(MemState &mem, ThreadState &thread, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    // The owner's own relock takes or fails at once, like its lock
    if (owner == thread.id)
        return try_take(mem, thread, 1);

    const SceUInt32 type = wait_type(lightweight() ? SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX : SCE_KERNEL_WAITTYPE_COND_MUTEX, callbacks);
    SceInt32 r;
    while (true) {
        r = run_wait_callbacks(guard, thread, callbacks);
        if (r != SCE_KERNEL_OK)
            break;
        // The same take as a lock
        if (!owner && (lightweight() || waiters.first_for(thread.get_priority()))) {
            r = try_take(mem, thread, 1);
            break;
        }
        r = Waitable::wait(waiters, guard, thread, type, { .count = 1 }, Deadline::max(), nullptr);
        if (r != SCE_KERNEL_ERROR_NOTIFY_CALLBACK) {
            // Only a delete runs the callbacks notified during the wait
            if (r == SCE_KERNEL_ERROR_WAIT_DELETE)
                r = finish_wait(guard, thread, callbacks, r);
            break;
        }
    }
    if (owner == thread.id)
        update_workarea(mem);
    return r;
}

SceInt32 Mutex::try_acquire(MemState &mem, ThreadState &thread, SceInt32 count) {
    const auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    if (const SceInt32 error = check_count(count))
        return error;

    return try_take(mem, thread, count);
}

SceInt32 Mutex::release(MemState &mem, ThreadState &thread, SceInt32 unlock_count) {
    const auto guard = lock();
    // Deleted meanwhile, which a lightweight mutex reports as not owned
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;

    if (const SceInt32 error = check_count(unlock_count))
        return error;
    if (thread.id != owner)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED : SCE_KERNEL_ERROR_MUTEX_NOT_OWNED;
    if (unlock_count > lock_count)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF : SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF;

    lock_count -= unlock_count;
    if (lock_count == 0)
        clear_owner();
    serve();
    update_workarea(mem);

    return SCE_KERNEL_OK;
}

void Mutex::release_if_owned_by(ThreadState &thread) {
    const auto guard = lock();
    if (!guard || owner_thread != &thread)
        return;
    lock_count = 0;
    clear_owner();
    serve();
}

void Mutex::serve() {
    if (owner)
        return;
    if (ThreadState *const waiter = waiters.front()) {
        lock_count = waiter->wait_request().count;
        set_owner(*waiter);
        waiters.wake(*waiter);
    }
}

SceInt32 Mutex::cancel(ThreadState &thread, SceInt32 new_count, SceUInt32 *num_wait_threads) {
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
    if (new_count > 0)
        set_owner(thread);
    else
        clear_owner();
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
    std::ranges::copy(name, info.name);
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
    std::ranges::copy(name, info.name);
    info.attr = attr;
    info.initCount = init_count;
    info.currentCount = lock_count;
    info.currentOwnerId = owner;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    if (attr & SCE_KERNEL_MUTEX_ATTR_CEILING)
        info.ceilingPriority = ceiling;
    return SCE_KERNEL_OK;
}

// **************
// * RWLock *
// **************

RWLock::RWLock(SceUInt32 attr, const char *name)
    : WithUidClass(attr, name) {}

SceInt32 RWLock::check_create(const char *name, SceUInt32 attr) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (attr & ~(SCE_KERNEL_RW_LOCK_ATTR_RECURSIVE | SCE_KERNEL_RW_LOCK_ATTR_CEILING | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    return SCE_KERNEL_OK;
}

void RWLock::end_waits(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    clear_write_owner();
}

void RWLock::set_write_owner(ThreadState &thread) {
    if (write_owner == &thread)
        return;
    if (write_owner)
        write_owner->remove_held(*this);
    write_owner = &thread;
    thread.add_held(RWLockRef(this));
}

void RWLock::clear_write_owner() {
    if (!write_owner)
        return;
    write_owner->remove_held(*this);
    write_owner = nullptr;
}

SceInt32 RWLock::try_take(ThreadState &thread, bool is_write) {
    // The writer may take the write lock again only if the lock is recursive
    if (is_write && write_owner == &thread) {
        if (!(attr & SCE_KERNEL_RW_LOCK_ATTR_RECURSIVE))
            return SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE;
        lock_count++;
        return SCE_KERNEL_OK;
    }

    // A waiting writer goes before new readers
    const bool writer_waits = waiters.find_if([](auto &waiter) { return waiter.wait_request().write; }) != nullptr;
    if (!is_write && writer_waits)
        return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;

    // Readers share the lock, but a reader and a writer don't, even in the same thread
    if (write_owner || (is_write && lock_count > 0))
        return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;

    lock_count++;
    if (is_write)
        set_write_owner(thread);
    return SCE_KERNEL_OK;
}

SceInt32 RWLock::acquire(ThreadState &thread, bool is_write, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
    // The writer locking again succeeds or fails at once, without callbacks. A reader asking for a
    // write lock waits, since readers aren't tracked individually.
    if (write_owner == &thread)
        return try_take(thread, is_write);

    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_RW_LOCK, callbacks);
    const Deadline deadline = deadline_from(timeout);
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return r;
        // A writer needs the lock to be free. A reader needs no writer and an empty queue. A lock
        // taken by this thread's callbacks counts like anyone else's.
        if (!write_owner && (is_write ? lock_count == 0 : waiters.empty())) {
            lock_count++;
            if (is_write)
                set_write_owner(thread);
            return SCE_KERNEL_OK;
        }
        const SceInt32 result = Waitable::wait(waiters, guard, thread, type, { .write = is_write }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return finish_wait(guard, thread, callbacks, result);
    }
}

SceInt32 RWLock::try_acquire(ThreadState &thread, bool is_write) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    return try_take(thread, is_write);
}

SceInt32 RWLock::try_acquire_read(ThreadState &thread) {
    return try_acquire(thread, false);
}

SceInt32 RWLock::try_acquire_write(ThreadState &thread) {
    return try_acquire(thread, true);
}

SceInt32 RWLock::acquire_read(ThreadState &thread, SceUInt32 *timeout, bool callbacks) {
    return acquire(thread, false, timeout, callbacks);
}

SceInt32 RWLock::acquire_write(ThreadState &thread, SceUInt32 *timeout, bool callbacks) {
    return acquire(thread, true, timeout, callbacks);
}

SceInt32 RWLock::release_read(ThreadState &thread) {
    return release(thread, false);
}

SceInt32 RWLock::release_write(ThreadState &thread) {
    return release(thread, true);
}

SceInt32 RWLock::release(ThreadState &thread, bool is_write) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    if (is_write) {
        // Only the writer may write unlock
        if (write_owner != &thread)
            return SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED;
    } else {
        if (write_owner)
            return SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK;
        if (lock_count == 0)
            return SCE_KERNEL_ERROR_RW_LOCK_UNLOCK_UDF;
    }

    if (--lock_count == 0)
        clear_write_owner();
    serve();
    return SCE_KERNEL_OK;
}

void RWLock::release_if_write_locked_by(ThreadState &thread) {
    const auto guard = lock();
    if (!guard || write_owner != &thread)
        return;
    lock_count = 0;
    clear_write_owner();
    serve();
}

SceInt32 RWLock::info(SceKernelRWLockInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    info = {};
    info.rwLockId = uid;
    std::ranges::copy(name, info.name);
    info.attr = attr;
    info.lockCount = lock_count;
    info.writeOwnerId = write_owner ? write_owner->id : 0;
    SceUInt32 writers = 0;
    for (const ThreadState &waiter : waiters) {
        if (waiter.wait_request().write)
            ++writers;
    }
    info.numReadWaitThreads = static_cast<SceUInt32>(waiters.size()) - writers;
    info.numWriteWaitThreads = writers;
    return SCE_KERNEL_OK;
}

void RWLock::serve() {
    if (write_owner)
        return;

    // Without a waiting writer every waiting reader gets in, also next to readers that hold the lock.
    // A waiting writer goes first, even before readers that waited longer, once nobody holds it.
    auto *const writer = waiters.find_if([](auto &waiter) { return waiter.wait_request().write; });
    if (!writer) {
        lock_count += static_cast<SceInt32>(waiters.wake_all());
    } else if (lock_count == 0) {
        set_write_owner(*writer);
        lock_count = 1;
        waiters.wake(*writer);
    }
}

SceInt32 RWLock::cancel(ThreadState &thread, SceUInt32 *num_read_wait_threads, SceUInt32 *num_write_wait_threads, SceInt32 flag) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    const auto is_writer = [](auto &waiter) { return waiter.wait_request().write; };
    const auto nb_writers = static_cast<SceUInt32>(waiters.wake_if(is_writer, SCE_KERNEL_ERROR_WAIT_CANCEL));
    const auto nb_readers = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));

    // The lock is reset, and only the calling thread may hold it afterwards
    const bool write_locked = flag & SCE_KERNEL_RW_LOCK_CANCEL_WITH_WRITE_LOCK;
    lock_count = write_locked ? 1 : 0;
    if (write_locked)
        set_write_owner(thread);
    else
        clear_write_owner();

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

void Semaphore::end_waits(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 Semaphore::wait(ThreadState &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    if (need_count <= 0 || need_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_SEMAPHORE, callbacks);
    const Deadline deadline = deadline_from(timeout);
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return r;
        if (try_take(thread, need_count))
            return SCE_KERNEL_OK;
        const SceInt32 result = Waitable::wait(waiters, guard, thread, type, { .count = need_count }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return finish_wait(guard, thread, callbacks, result);
    }
}

SceInt32 Semaphore::poll(ThreadState &thread, SceInt32 need_count) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    if (need_count <= 0 || need_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    return try_take(thread, need_count) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_SEMA_ZERO;
}

bool Semaphore::try_take(ThreadState &thread, SceInt32 need_count) {
    // A waiter queued ahead goes first, even one the count doesn't cover yet
    if (val < need_count || !waiters.first_for(thread.get_priority()))
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
    while (ThreadState *const waiter = waiters.front()) {
        if (val < waiter->wait_request().count)
            break;
        val -= waiter->wait_request().count;
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
    std::ranges::copy(name, info.name);
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

Condvar::Condvar(SceUInt32 attr, const char *name, MutexRef associated_mutex)
    : Waitable(attr, name)
    , associated_mutex(std::move(associated_mutex)) {}

void Condvar::end_waits(SceUID) {
    // wait() maps this to the condition's own delete error
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 Condvar::wait(MemState &mem, ThreadState &thread, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID : SCE_KERNEL_ERROR_UNKNOWN_COND_ID;

    if (const SceInt32 error = associated_mutex->release(mem, thread, 1))
        return error;

    // Condition waits have nothing to take, they only wait for a signal
    const SceUInt32 type = wait_type(lightweight() ? SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL : SCE_KERNEL_WAITTYPE_COND_SIGNAL, callbacks);
    const Deadline deadline = deadline_from(timeout);
    SceInt32 r;
    while (true) {
        r = run_wait_callbacks(guard, thread, callbacks);
        if (r != SCE_KERNEL_OK)
            break;
        r = Waitable::wait(waiters, guard, thread, type, {}, deadline, timeout);
        if (r != SCE_KERNEL_ERROR_NOTIFY_CALLBACK) {
            // Only a delete runs the callbacks notified during the wait
            if (r == SCE_KERNEL_ERROR_WAIT_DELETE)
                r = finish_wait(guard, thread, callbacks, r);
            break;
        }
    }
    // Deleted during the wait, or while its callbacks ran
    if (r == SCE_KERNEL_ERROR_WAIT_DELETE)
        return lightweight() ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND : SCE_KERNEL_ERROR_WAIT_DELETE_COND;
    // A timed out wait still takes the mutex back
    const bool timed_out = r == SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    if (r != SCE_KERNEL_OK && !timed_out)
        return r;

    guard.unlock();
    // Taking the mutex back is still part of the condition variable wait, a wait on the mutex with no
    // timeout that leaves the time left as it was at the signal
    const SceInt32 result = associated_mutex->relock(mem, thread, callbacks);
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
        auto *waiter = waiters.find_if([&](auto &w) { return w.id == target.thread_id; });
        if (!waiter)
            return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
        waiters.wake(*waiter);
    } else if (target.type == SignalTarget::Type::Any) {
        // Wakes the first waiter in queue order
        if (ThreadState *const waiter = waiters.front())
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
    std::ranges::copy(name, info.name);
    info.attr = attr;
    info.mutexId = associated_mutex->uid;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
}

LwCond::LwCond(SceUInt32 attr, const char *name, MutexRef associated_mutex, Ptr<SceKernelLwCondWork> workarea)
    : WithUidClass(attr, name, std::move(associated_mutex))
    , workarea(workarea) {}

SceInt32 LwCond::info(SceKernelLwCondInfo &info) const {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID;

    info = {};
    info.uid = uid;
    std::ranges::copy(name, info.name);
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

void EventFlag::end_waits(SceUID) {
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    clear_holder();
    holds = 0;
}

void EventFlag::wait_end(ThreadState &thread, SceInt32 result) {
    if (result != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
        return;
    if (thread.wait_request().out_pattern)
        *thread.wait_request().out_pattern = flags;
    drop_hold();
}

void EventFlag::drop_hold() {
    if (holds > 0 && --holds == 0)
        clear_holder();
}

void EventFlag::set_holder(ThreadState &thread) {
    assert(!holder);
    holder = &thread;
    thread.add_held(EventFlagRef(this));
}

void EventFlag::clear_holder() {
    if (!holder)
        return;
    holder->remove_held(*this);
    holder = nullptr;
}

bool EventFlag::held_by_other(SceUID thread_id) const {
    return holds > 0 && holder->id != thread_id;
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

SceInt32 EventFlag::wait(ThreadState &thread, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits, SceUInt32 *timeout, bool callbacks) {
    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    if (const SceInt32 error = check_wait(pattern, wait_mode))
        return error;
    if (held_by_other(thread.id))
        return SCE_KERNEL_ERROR_EVF_MULTI;

    // A flag without SCE_EVENT_WAITMULTIPLE is held while the wait lasts, take() releases it
    if (!(attr & SCE_EVENT_WAITMULTIPLE) && holds++ == 0)
        set_holder(thread);
    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_EVENTFLAG, callbacks);
    const WaitRequest request{ .pattern = pattern, .mode = wait_mode, .out_pattern = out_bits };
    const Deadline deadline = deadline_from(timeout);
    SceInt32 r;
    while (true) {
        r = run_wait_callbacks(guard, thread, callbacks);
        if (r != SCE_KERNEL_OK)
            break;
        if (take(request))
            break;
        // A wait whose deadline is already gone returns these bits
        if (out_bits)
            *out_bits = flags;
        r = Waitable::wait(waiters, guard, thread, type, request, deadline, timeout);
        if (r != SCE_KERNEL_ERROR_NOTIFY_CALLBACK) {
            r = finish_wait(guard, thread, callbacks, r);
            break;
        }
    }
    // A deleted flag's bits can't change any more, so a wait that a delete ended gets them when it
    // returns, also one deleted while its callbacks ran out of the queue
    if (r == SCE_KERNEL_ERROR_WAIT_DELETE && out_bits)
        *out_bits = flags;
    return r;
}

SceInt32 EventFlag::poll(SceUID thread_id, SceUInt32 pattern, SceUInt32 wait_mode, SceUInt32 *out_bits) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    if (const SceInt32 error = check_wait(pattern, wait_mode))
        return error;

    return try_take(thread_id, pattern, wait_mode, out_bits);
}

void EventFlag::release_if_held_by(ThreadState &thread) {
    const auto guard = lock();
    if (!guard || holder != &thread)
        return;
    holds = 0;
    clear_holder();
}

SceInt32 EventFlag::set(SceUInt32 pattern) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    flags |= pattern;
    serve();

    return SCE_KERNEL_OK;
}

bool EventFlag::take(const WaitRequest &wait) {
    const bool condition = (wait.mode & SCE_EVENT_WAITOR) ? (flags & wait.pattern) != 0 : (flags & wait.pattern) == wait.pattern;
    if (!condition)
        return false;

    if (wait.out_pattern)
        *wait.out_pattern = flags;
    if (wait.mode & SCE_EVENT_WAITCLEAR)
        flags = 0;
    if (wait.mode & SCE_EVENT_WAITCLEAR_PAT)
        flags &= ~wait.pattern;
    drop_hold();
    return true;
}

void EventFlag::serve() {
    waiters.wake_if([&](auto &waiter) { return take(waiter.wait_request()); });
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
        if (waiter.wait_request().out_pattern)
            *waiter.wait_request().out_pattern = pattern;
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
    std::ranges::copy(name, info.name);
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
    : WithUidClass(attr, attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_S ? WaitQueue::Order::priority : WaitQueue::Order::fifo, name)
    , receivers(attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_R ? WaitQueue::Order::priority : WaitQueue::Order::fifo)
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

void MsgPipe::end_waits(SceUID) {
    // Wake up every thread
    waiters.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    receivers.wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

// The size of all the pieces together
static uint64_t total_size(std::span<const MsgPipeVector> pieces) {
    uint64_t total = 0;
    for (const MsgPipeVector &piece : pieces)
        total += piece.size;
    return total;
}

MsgPipeTransfer::MsgPipeTransfer(MemState &mem, std::span<const MsgPipeVector> pieces, SceSize *size, SceSize total, bool full)
    : pieces(pieces)
    , mem(&mem)
    , size(size)
    , total(total)
    , left(total)
    , full(full) {}

MsgPipeReceiver::MsgPipeReceiver(MemState &mem, std::span<const MsgPipeVector> pieces, SceSize *size, SceSize total, bool full, bool peek)
    : MsgPipeTransfer(mem, pieces, size, total, full)
    , peek(peek) {}

bool MsgPipeTransfer::done() const {
    return full ? left == 0 : left < total;
}

bool MsgPipeTransfer::keeps_place() const {
    return left != total;
}

std::span<char> MsgPipeTransfer::run(SceSize n) {
    // Steps over the pieces already moved, and empty ones
    while (offset == pieces[piece].size) {
        ++piece;
        offset = 0;
    }
    const MsgPipeVector &current = pieces[piece];
    return std::span{ current.base.cast<char>().get(*mem), current.size }.subspan(offset, std::min(n, current.size - offset));
}

void MsgPipeTransfer::advance(SceSize n) {
    offset += n;
    left -= n;
}

void MsgPipeTransfer::report() const {
    if (size)
        *size = total - left;
}

void MsgPipe::copy(MsgPipeSender &sender, MsgPipeReceiver &receiver, SceSize n) {
    // A copy of the sender moves through its pieces, and a peek leaves the sender where it was
    MsgPipeSender from = sender;
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

void MsgPipe::fill(MsgPipeSender &sender) {
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

void MsgPipe::drain(MsgPipeReceiver &receiver) {
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
    for (const ThreadState &sender : waiters)
        bytes += sender.wait_request().sender->left;
    return bytes;
}

bool MsgPipe::give(MsgPipeSender &sender) {
    // A send reaches the first waiting receiver even with no bytes, which ends an ASAP receive
    while (ThreadState *const receiver = receivers.front()) {
        MsgPipeReceiver &to = *receiver->wait_request().receiver;
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

bool MsgPipe::take(MsgPipeReceiver &receiver) {
    drain(receiver);
    // Each waiting sender gives its bytes to the receiver while it wants some, then puts its rest
    // into the pipe, up to the first sender that stays
    while (ThreadState *const sender = waiters.front()) {
        MsgPipeSender &from = *sender->wait_request().sender;
        copy(from, receiver, std::min(receiver.left, from.left));
        // A peek frees nothing
        if (receiver.peek)
            break;
        fill(from);
        from.report();
        if (!from.done())
            break;
        waiters.wake(*sender);
    }
    receiver.report();
    return receiver.done();
}

void MsgPipe::serve() {
    // The first receiver goes first, so the pipe's bytes go out before those of waiting senders
    while (true) {
        if (ThreadState *const receiver = receivers.front(); receiver && take(*receiver->wait_request().receiver))
            receivers.wake(*receiver);
        else if (ThreadState *const sender = waiters.front(); sender && give(*sender->wait_request().sender))
            waiters.wake(*sender);
        else
            break;
    }
}

bool MsgPipe::goes_first(WaitQueue &side, int priority) {
    // Nothing goes ahead of a head that moved part of its bytes
    const ThreadState *const head = side.front();
    return side.first_for(priority) && (!head || !keeps_place(head->wait_request()));
}

SceInt32 MsgPipe::receive(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks) {
    if (!is_msg_pipe_mode(wait_mode))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    // DONT_WAIT ends like a try, and a wait with callbacks runs them after it
    if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT) {
        const SceInt32 result = try_receive(thread, mem, wait_mode, pieces, size);
        if (callbacks)
            thread.process_callbacks();
        return result;
    }
    const uint64_t total = total_size(pieces);
    if (total > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    MsgPipeReceiver receiver(mem, pieces, size, static_cast<SceSize>(total), wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE);
    // A receive of nothing ends at once, before any callback runs
    if (receiver.total == 0) {
        receiver.report();
        return SCE_KERNEL_OK;
    }

    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_MSG_PIPE, callbacks);
    const Deadline deadline = deadline_from(timeout);
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return r;
        // Don't take anything while other receivers are ahead in the queue. The size is written
        // after every attempt.
        const bool done = goes_first(receivers, thread.get_priority()) && take(receiver);
        receiver.report();
        // The callbacks notified meanwhile run also after a receive that got its bytes
        if (done)
            return finish_wait(guard, thread, callbacks, SCE_KERNEL_OK);
        const SceInt32 result = Waitable::wait(receivers, guard, thread, type, { .receiver = &receiver }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return finish_wait(guard, thread, callbacks, result);
    }
}

SceInt32 MsgPipe::send(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks) {
    if (!is_msg_pipe_mode(wait_mode))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    // DONT_WAIT ends like a try, and a wait with callbacks runs them after it
    if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT) {
        const SceInt32 result = try_send(thread, mem, wait_mode, pieces, size);
        if (callbacks)
            thread.process_callbacks();
        return result;
    }
    const uint64_t total = total_size(pieces);
    if (total > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    MsgPipeSender sender(mem, pieces, size, static_cast<SceSize>(total), wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);
    // A send of nothing ends at once, before any callback runs
    if (sender.total == 0) {
        sender.report();
        return SCE_KERNEL_OK;
    }

    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_MSG_PIPE, callbacks);
    const Deadline deadline = deadline_from(timeout);
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return r;
        // Don't send anything while other senders are ahead in the queue. The size is written
        // after every attempt.
        const bool done = goes_first(waiters, thread.get_priority()) && give(sender);
        sender.report();
        // The callbacks notified meanwhile run also after a send that moved its bytes
        if (done)
            return finish_wait(guard, thread, callbacks, SCE_KERNEL_OK);
        const SceInt32 result = Waitable::wait(waiters, guard, thread, type, { .sender = &sender }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return finish_wait(guard, thread, callbacks, result);
    }
}

SceInt32 MsgPipe::try_receive(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size) {
    if (!is_msg_pipe_mode(wait_mode))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    const uint64_t total = total_size(pieces);
    if (total > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    MsgPipeReceiver receiver(mem, pieces, size, static_cast<SceSize>(total), wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE);
    // Behind the receivers already waiting, or short of bytes for all of a FULL receive, it fails
    // without writing a size
    if (!goes_first(receivers, thread.get_priority()) || (receiver.full && receiver.total > available()))
        return SCE_KERNEL_ERROR_MSG_PIPE_EMPTY;
    return take(receiver) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_MSG_PIPE_EMPTY;
}

SceInt32 MsgPipe::try_send(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size) {
    if (!is_msg_pipe_mode(wait_mode))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    const uint64_t total = total_size(pieces);
    if (total > data_buffer.Capacity())
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    MsgPipeSender sender(mem, pieces, size, static_cast<SceSize>(total), wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);
    // Behind the senders already waiting, or short of room for all of a FULL send, it fails without
    // writing a size
    if (!goes_first(waiters, thread.get_priority()) || (sender.full && sender.total > data_buffer.Free()))
        return SCE_KERNEL_ERROR_MSG_PIPE_FULL;
    return give(sender) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_MSG_PIPE_FULL;
}

SceInt32 MsgPipe::cancel(SceUInt32 *num_send_wait_threads, SceUInt32 *num_receive_wait_threads) {
    const auto guard = lock();
    if (!guard)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;

    const auto nb_senders = static_cast<SceUInt32>(waiters.wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
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
    std::ranges::copy(name, info.name);
    info.attr = attr;
    info.bufSize = static_cast<SceInt32>(data_buffer.Capacity());
    info.freeSize = static_cast<SceInt32>(data_buffer.Free());
    info.numSendWaitThreads = static_cast<SceInt32>(waiters.size());
    info.numReceiveWaitThreads = static_cast<SceInt32>(receivers.size());
    return SCE_KERNEL_OK;
}
