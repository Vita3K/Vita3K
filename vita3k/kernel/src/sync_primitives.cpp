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

// Whether a waiter with request queues by priority on an object with attr. A message pipe orders
// each side by its own bit.
static bool by_priority(SceUInt32 attr, const WaitRequest &request) {
    if (request.sender)
        return attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_S;
    if (request.receiver)
        return attr & SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO_R;
    return attr & SCE_KERNEL_ATTR_TH_PRIO;
}

ThreadState *Waitable::first_waiter() {
    return waiters.empty() ? nullptr : &waiters.front();
}

bool Waitable::first_for(int priority) const {
    if (waiters.empty())
        return true;
    // Lower value is higher priority, and equal priorities keep FIFO order
    return (attr & SCE_KERNEL_ATTR_TH_PRIO) && waiters.front().queued_priority > priority;
}

void Waitable::wake_waiter(ThreadState &thread, SceInt32 result) {
    const std::lock_guard<std::mutex> thread_lock(thread.thread_mutex);
    wake_one(thread, this, result);
}

void Waitable::wake_one(ThreadState &thread, Waitable *queue, SceInt32 result) {
    if (queue) {
        queue->waiters.remove(thread);
        thread.waited = nullptr;
    }
    // The result and the time left are stored before the class's hook runs
    thread.wait_result = result;
    if (thread.time_left)
        *std::exchange(thread.time_left, nullptr) = time_left_until(thread.wait_deadline);
    if (queue)
        queue->wait_ended(thread, result);
    thread.make_ready();
}

std::size_t Waitable::wake_all(SceInt32 result) {
    return wake_all_if([](ThreadState &) { return true; }, result);
}

SceInt32 Waitable::queue(ThreadState &thread, SceUInt32 type, const WaitRequest &request, Deadline deadline, SceUInt32 *timeout) {
    assert(!thread.wait_link.linked());
    const std::lock_guard<std::mutex> thread_lock(thread.thread_mutex);
    // The thread's end comes first, then a deadline already gone, then a pending callback
    if (thread.exiting())
        return SCE_KERNEL_ERROR_THREAD_STOPPED;
    // A deadline already gone doesn't queue and leaves no time
    if (std::chrono::steady_clock::now() >= deadline) {
        writeback_timeout(timeout, deadline);
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    }
    // A callback already pending leaves the timeout as it was
    if ((type & SCE_KERNEL_WAITTYPE_CB_BIT) && thread.callback_pending)
        return SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
    // Only a wait that queues adds its side counts, so a refused one has none to undo. The hook reads
    // the priority the thread queues with.
    thread.request = request;
    thread.queued_priority = thread.priority;
    waiter_added(thread);
    // The thread shows the wait from here, and its link and its target change together
    thread.time_left = deadline == Deadline::max() ? nullptr : timeout;
    thread.wait_deadline = deadline;
    thread.waited = this;
    thread.enter_wait({ type, uid });
    if (attr & SCE_KERNEL_ATTR_TH_PRIO)
        insert_waiter_by_priority(thread);
    else
        waiters.push_back(thread);
    return SCE_KERNEL_OK;
}

void Waitable::insert_waiter_by_priority(ThreadState &thread) {
    // Lower value is higher priority, equal priorities keep FIFO order
    auto pos = waiters.begin();
    while (pos != waiters.end() && pos->queued_priority <= thread.queued_priority)
        ++pos;
    // A thread whose place is the front goes right behind a head that keeps its place, so it
    // lands ahead of the threads already behind that head, whatever their priority
    if (pos == waiters.begin() && pos != waiters.end() && queues_behind_head(thread))
        ++pos;
    waiters.insert(pos, thread);
}

void Waitable::requeue(ThreadState &thread) {
    // A FIFO queue keeps its order, and the same priority still moves the thread behind its equals
    if (attr & SCE_KERNEL_ATTR_TH_PRIO) {
        waiters.remove(thread);
        insert_waiter_by_priority(thread);
    }
}

Waitable::Waitable(SceUInt32 attr, const char *name)
    : WithClass(name)
    , attr(attr)
    , openable(attr & SCE_KERNEL_ATTR_OPENABLE) {}

void Waitable::on_delete(SceUID deleter) {
    std::unique_lock<std::mutex> guard(mutex);
    assert(!deleted);
    deleted = true;
    end_waits(deleter, guard);
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

SceInt32 Waitable::wait(std::unique_lock<std::mutex> &lock, ThreadState &thread, SceUInt32 type, const WaitRequest &request, Deadline deadline, SceUInt32 *timeout) {
    if (const SceInt32 refused = queue(thread, type, request, deadline, timeout))
        return refused;
    lock.unlock();
    SceInt32 result;
    {
        // Whoever ends the wait takes the thread off the queue, writes the time left and serves the
        // others
        std::unique_lock<std::mutex> thread_lock(thread.thread_mutex);
        result = thread.sleep(thread_lock, deadline);
    }
    lock.lock();
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
    : WithClass(attr, name)
    , auto_reset(attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
    , pattern(init_pattern | SCE_KERNEL_EVENT_CREATE)
    , last_user_data(init_user_data) {}

Event::~Event() {
    while (!registrations.empty())
        drop_registration(registrations.front());
}

// The create event comes with the creating thread's user data, like the other system events
SimpleEvent::SimpleEvent(SceUInt32 attr, const char *name, SceUInt32 init_pattern, SceUID creator)
    : WithUidClass(attr, name, init_pattern, system_event_user_data(creator)) {}

SceInt32 SimpleEvent::check_create(const char *name, SceUInt32 attr, SceUInt32 init_pattern, const SceKernelSimpleEventOptParam *opt) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    // The notify attribute is accepted and reported, nothing reads it
    if (attr & ~(SCE_KERNEL_EVENT_ATTR_AUTO_RESET | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE | SCE_KERNEL_ATTR_NOTIFY_CB_WAKEUP_ONLY))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (opt && opt->size > sizeof(*opt))
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    if (init_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
    return SCE_KERNEL_OK;
}

void Event::end_waits(SceUID deleter, std::unique_lock<std::mutex> &guard) {
    // The waits for the delete event get it with the deleting thread's user data, then the others end
    fire(SCE_KERNEL_EVENT_DELETE, system_event_user_data(deleter), true);
    notify_event_callbacks(guard, SCE_KERNEL_EVENT_DELETE);
    wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 Event::register_callback(KernelState &kernel, CallbackRef callback, SceUID notify_id) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    // After the registrations of the same callback
    auto pos = registrations.begin();
    while (pos != registrations.end() && (!pos->callback || pos->callback->uid <= callback->uid))
        ++pos;
    CallbackRegistration *const registration = new CallbackRegistration(kernel, std::move(callback), notify_id);
    registrations.insert(pos, *registration);
    return SCE_KERNEL_OK;
}

SceInt32 Event::unregister_callback(SceUID callback_id) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    for (CallbackRegistration &registration : registrations) {
        if (registration.callback && registration.callback->uid == callback_id) {
            drop_registration(registration);
            return SCE_KERNEL_OK;
        }
    }
    return SCE_KERNEL_ERROR_CALLBACK_NOT_REGISTERED;
}

SceInt32 Event::unregister_callbacks() {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    // Leaves the places of walks in progress
    for (auto it = registrations.begin(); it != registrations.end();) {
        CallbackRegistration &registration = *it++;
        if (registration.callback)
            drop_registration(registration);
    }
    return SCE_KERNEL_OK;
}

void Event::notify_event_callbacks(std::unique_lock<std::mutex> &guard, SceInt32 arg) {
    for (auto it = registrations.begin(); it != registrations.end();) {
        CallbackRegistration &registration = *it++;
        // Another walk's place
        if (!registration.callback)
            continue;
        Callback &callback = *registration.callback;
        const ThreadStateRef owner = registration.kernel.get_thread(callback.thread_id);
        std::unique_lock<std::mutex> lock;
        if (owner)
            lock = std::unique_lock<std::mutex>(owner->thread_mutex);
        // A callback whose owner is gone, or that was closed, is dropped
        if (!owner || !callback.owner_link.linked()) {
            drop_registration(registration);
            continue;
        }
        callback.note(arg, registration.notify_id);
        owner->callback_pending = true;
        // An owner that doesn't wait with callbacks only keeps the notification
        if (!(owner->status == ThreadStatus::waiting && (owner->wait_target.type & SCE_KERNEL_WAITTYPE_CB_BIT)))
            continue;
        // A wait on this event ends with the walk's lock and no serve, and a wait on nothing needs none
        Waitable *const object = owner->waited;
        if (!object || object == this) {
            Waitable::wake_one(*owner, object, SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
            continue;
        }
        // A wait on another object ends with the event let go, since its lock can't come before
        // theirs. The walk keeps its place in the list meanwhile with a registration of no callback.
        CallbackRegistration place(registration.kernel, nullptr, 0);
        auto at = registrations.insert(it, place);
        guard.unlock();
        // Read in the step, since the wait may end while no lock is held
        owner->change_wait(lock, [&](Waitable *queue) {
            if (queue && owner->status == ThreadStatus::waiting && (owner->wait_target.type & SCE_KERNEL_WAITTYPE_CB_BIT) && owner->callback_pending)
                Waitable::wake_one(*owner, queue, SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
        });
        guard.lock();
        it = ++at;
        registrations.remove(place);
    }
}

void Event::drop_registration(CallbackRegistration &registration) {
    registrations.remove(registration);
    delete &registration;
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

SceInt32 Event::wait(ThreadState &thread, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool callbacks) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
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
        result = Waitable::wait(guard, thread, type, { .pattern = wait_pattern, .out_pattern = result_pattern, .out_user_data = user_data }, deadline, timeout);
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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (wait_pattern == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    return try_take(wait_pattern, result_pattern, user_data);
}

SceInt32 Event::set_or_pulse(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (set_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;

    fire(set_pattern, user_data, is_set);
    return SCE_KERNEL_OK;
}

SceInt32 Event::set_or_pulse_and_notify(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set, SceInt32 notify_arg) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (set_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
    if (set_pattern == 0)
        return SCE_KERNEL_OK;

    // The waiters get the bits before the callbacks are notified
    fire(set_pattern, user_data, is_set);
    notify_event_callbacks(guard, notify_arg);
    return SCE_KERNEL_OK;
}

void SimpleEvent::pulse_and_notify(SceUInt32 pattern, SceInt32 notify_arg) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return;
    fire(pattern, 0, false);
    notify_event_callbacks(guard, notify_arg);
}

SceInt32 Event::set_system_event(SceUInt32 event, SceUInt64 user_data) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    fire(event, user_data, true);
    // The callbacks get the event's bit as the argument
    notify_event_callbacks(guard, event);
    return SCE_KERNEL_OK;
}

void Event::fire(SceUInt32 set_pattern, SceUInt64 user_data, bool is_set) {
    // A set or pulse of no bits changes nothing, not even the user data
    if (set_pattern == 0)
        return;

    pattern |= set_pattern;
    last_user_data = user_data;
    Event::serve();

    // A pulse clears its bits once the waiters had them, also the ones set before it, and keeps its
    // user data
    if (!is_set)
        pattern &= ~set_pattern;
}

void Event::serve() {
    wake_all_if([&](auto &waiter) {
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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (clear_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;

    pattern &= clear_pattern;

    return SCE_KERNEL_OK;
}

SceInt32 Event::cancel(SceUInt32 *num_wait_threads) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

    const auto nb_threads = static_cast<SceUInt32>(wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

SceInt32 Event::cancel(SceUInt32 set_pattern, SceUInt64 user_data, SceUInt32 *num_wait_threads) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    if (set_pattern & ~SCE_KERNEL_EVENT_USER_DEFINED_MASK)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;

    const auto nb_threads = static_cast<SceUInt32>(wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    // Every bit is replaced, the create event too, and no waiter is left to get them
    pattern = set_pattern;
    last_user_data = user_data;
    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

SceInt32 Event::get_pattern(SceUInt32 &result) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    result = pattern;
    return SCE_KERNEL_OK;
}

SceInt32 Event::info(SceKernelEventInfo &info) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;

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

inline uint64_t timer_clock() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

Timer::Timer(TimerService &timers, SceUInt32 attr, const char *name, SceUID creator)
    : WithUidClass(attr, name, 0, system_event_user_data(creator))
    , timers(timers) {}

SceInt32 Timer::check_create(const char *name, SceUInt32 attr) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (attr & ~(SCE_KERNEL_EVENT_ATTR_AUTO_RESET | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE | SCE_KERNEL_ATTR_NOTIFY_CB_WAKEUP_ONLY))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    return SCE_KERNEL_OK;
}

// Minimum time until the worker starts its next batch.
constexpr uint64_t TIMER_EVENT_FLOOR = 200;
// Longest single wait of the worker, so a far deadline is waited for in steps
constexpr uint64_t TIMER_MAX_WAIT = 1000000;

TimerService::~TimerService() {
    stop();
}

void TimerService::start() {
    thread = std::thread(&TimerService::run, this);
}

void TimerService::stop() {
    {
        const std::lock_guard<std::mutex> guard(mutex);
        stopping = true;
    }
    wake.notify_one();
    if (thread.joinable())
        thread.join();
    const std::lock_guard<std::mutex> guard(mutex);
    stopping = false;
    while (!timers.empty())
        timers.remove(timers.front());
}

void TimerService::arm(Timer &timer, uint64_t deadline) {
    const std::lock_guard<std::mutex> guard(mutex);
    unlink(timer);
    timer.deadline = deadline;
    // After the timers due at the same time
    auto pos = timers.begin();
    while (pos != timers.end() && pos->deadline <= deadline)
        ++pos;
    const bool first = pos == timers.begin();
    timers.insert(pos, timer);
    if (first)
        wake.notify_one();
}

void TimerService::disarm(Timer &timer) {
    const std::lock_guard<std::mutex> guard(mutex);
    unlink(timer);
}

bool TimerService::take_due(Timer &timer, uint64_t cutoff) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (!timer.service_link.linked() || timer.deadline > cutoff)
        return false;
    unlink(timer);
    return true;
}

void TimerService::unlink(Timer &timer) {
    if (!timer.service_link.linked())
        return;
    // A batch that would fire it next moves on
    if (cursor != timers.end() && &*cursor == &timer)
        ++cursor;
    timers.remove(timer);
}

void TimerService::run() {
    std::unique_lock<std::mutex> lock(mutex);
    while (!stopping) {
        if (timers.empty()) {
            wake.wait(lock);
            continue;
        }
        // The next batch starts at the first deadline, but never sooner than the floor from now
        const uint64_t now = timer_clock();
        const uint64_t first = timers.begin()->deadline;
        const uint64_t wait = first > now ? std::min(first - now, TIMER_MAX_WAIT) : 0;
        if (wake.wait_for(lock, std::chrono::microseconds(std::max(wait, TIMER_EVENT_FLOOR))) != std::cv_status::timeout)
            continue;
        // Fires the timers due by this time in list order. A repeat armed again in front of the
        // cursor fires in the next batch, and a stop ends the batch.
        const uint64_t cutoff = timer_clock();
        for (auto it = timers.begin(); it != timers.end() && it->deadline <= cutoff && !stopping; it = cursor) {
            cursor = std::next(it);
            {
                // The timer's lock comes before the list's, so the list is unlocked to take it
                const TimerRef timer = Ref<Timer>::from(*it);
                lock.unlock();
                timer->expire(cutoff);
            }
            lock.lock();
        }
        cursor = timers.end();
    }
}

void Timer::expire(uint64_t cutoff) {
    std::unique_lock<std::mutex> guard(mutex);
    // A delete, stop, cancel or new event since the service took the timer can leave nothing due
    if (deleted || !timers.take_due(*this, cutoff))
        return;
    fire(SCE_KERNEL_EVENT_TIMER, 0, !is_pulse);
    // A one shot event stays armed, so a start or a set time arms it again
    if (is_repeat) {
        const uint64_t next = deadline + std::max(event_interval, TIMER_EVENT_FLOOR);
        schedule = next - base;
        timers.arm(*this, next);
    }
    notify_event_callbacks(guard, 0);
}

void Timer::end_waits(SceUID deleter, std::unique_lock<std::mutex> &guard) {
    timers.disarm(*this);
    Event::end_waits(deleter, guard);
}

SceInt32 Timer::set_event(SceUID type, SceKernelSysClock interval, SceInt32 repeats) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    if (type != SCE_KERNEL_TIMER_TYPE_SET_EVENT && type != SCE_KERNEL_TIMER_TYPE_PULSE_EVENT)
        return SCE_KERNEL_ERROR_ILLEGAL_TYPE;

    is_pulse = type == SCE_KERNEL_TIMER_TYPE_PULSE_EVENT;
    is_repeat = repeats != 0;
    event_interval = interval;
    armed = true;
    schedule = count() + interval;
    // A stopped timer queues the event when it starts
    if (is_started)
        timers.arm(*this, timer_clock() + interval);
    else
        timers.disarm(*this);

    return SCE_KERNEL_OK;
}

SceInt32 Timer::start() {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    if (is_started)
        return 1;

    // The count goes on from where it stopped, and an armed event is due after the time it has left,
    // at once if it already fired
    is_started = true;
    const uint64_t now = timer_clock();
    base = now - stopped_count;
    if (armed)
        timers.arm(*this, now + (schedule > stopped_count ? schedule - stopped_count : 0));

    return SCE_KERNEL_OK;
}

SceInt32 Timer::stop() {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    if (!is_started)
        return 0;

    stopped_count = count();
    is_started = false;
    timers.disarm(*this);

    return 1;
}

std::expected<uint64_t, SceInt32> Timer::set_time(uint64_t new_time) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return std::unexpected(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    const uint64_t old_count = count();
    if (is_started) {
        const uint64_t now = timer_clock();
        base = now - new_time;
        // If the new time is before the armed event's time, the event is due when the count reaches
        // it again. Otherwise it stays due when it was.
        if (armed && new_time < schedule)
            timers.arm(*this, now + (schedule - new_time));
    } else {
        stopped_count = new_time;
    }
    return old_count;
}

uint64_t Timer::count() const {
    return is_started ? timer_clock() - base : stopped_count;
}

SceInt32 Timer::get_time(uint64_t &time) const {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    time = count();
    return SCE_KERNEL_OK;
}

SceInt32 Timer::get_base(uint64_t &time) const {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    time = is_started ? base : 0;
    return SCE_KERNEL_OK;
}

SceInt32 Timer::remaining_time(uint64_t &time) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
    if (!armed)
        return SCE_KERNEL_ERROR_EVENT_NOT_SET;
    time = schedule - count();
    return SCE_KERNEL_OK;
}

SceInt32 Timer::timer_info(SceKernelTimerInfo &info) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    const auto nb_threads = static_cast<SceUInt32>(wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    // The event set with sceKernelSetTimerEvent is canceled too, but the timer keeps running with
    // the event's interval and type
    armed = false;
    is_repeat = false;
    timers.disarm(*this);
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

void Mutex::end_waits(SceUID, std::unique_lock<std::mutex> &guard) {
    // Freed before the lock is let go, since an ending owner only gives back a mutex that isn't deleted
    const ThreadStateRef freed = clear_owner();
    // Then the waits on its condition variables end, each with the lock let go since a condition
    // variable's lock comes first
    while (!conditions.empty()) {
        {
            const CondvarRef condition = CondvarRef::from(conditions.front());
            conditions.remove(*condition);
            guard.unlock();
            condition->end_mutex_waits();
        }
        // Taken again once the reference is gone, since a last one leaves the list under this lock
        guard.lock();
    }
    wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    // The owner runs at its new priority already, and moves in the queue of another object it may
    // wait on once the lock is let go
    guard.unlock();
    if (freed)
        freed->take_priority_place();
}

SceInt32 Mutex::add_condition(Condvar &condition) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    conditions.push_back(condition);
    return SCE_KERNEL_OK;
}

void Mutex::remove_condition(Condvar &condition) {
    // Also once the mutex was deleted, whose delete may not have reached the condition variable yet
    const std::lock_guard<std::mutex> guard(mutex);
    if (condition.mutex_link.linked())
        conditions.remove(condition);
}

void Mutex::set_owner(ThreadState &thread) {
    owner = thread.id;
    if (lightweight())
        return;
    assert(!owner_thread);
    owner_thread = &thread;
    thread.hold_mutex(*this);
}

ThreadStateRef Mutex::clear_owner() {
    owner = 0;
    if (!owner_thread)
        return {};
    // Taken while the owner still holds the mutex, since its end gives the mutex back first
    ThreadStateRef freed = ThreadStateRef::from(*std::exchange(owner_thread, nullptr));
    if (!freed->free_mutex(*this))
        return {};
    return freed;
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

void Mutex::count_failed_take(MemState &mem) {
    if (lightweight())
        ++workarea.get(mem)->waitCount;
}

void Mutex::update_workarea(MemState &mem) {
    if (!lightweight())
        return;
    workarea.get(mem)->lockCount = lock_count;
    workarea.get(mem)->owner = owner;
}

SceInt32 Mutex::acquire(MemState &mem, ThreadState &thread, SceInt32 count, SceUInt32 *timeout, bool callbacks) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
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
        if (!owner && (lightweight() || first_for(thread.get_priority()))) {
            r = try_take(mem, thread, count);
            break;
        }
        count_failed_take(mem);
        r = Waitable::wait(guard, thread, type, { .count = count }, deadline, timeout);
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
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
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
        if (!owner && (lightweight() || first_for(thread.get_priority()))) {
            r = try_take(mem, thread, 1);
            break;
        }
        count_failed_take(mem);
        r = Waitable::wait(guard, thread, type, { .count = 1 }, Deadline::max(), nullptr);
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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    if (const SceInt32 error = check_count(count))
        return error;

    return try_take(mem, thread, count);
}

SceInt32 Mutex::release(MemState &mem, ThreadState &thread, SceInt32 unlock_count) {
    const std::lock_guard<std::mutex> guard(mutex);
    // Deleted since the lookup, or before a condition variable wait's unlock
    if (deleted)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
    if (const SceInt32 error = check_count(unlock_count))
        return error;
    if (thread.id != owner)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED : SCE_KERNEL_ERROR_MUTEX_NOT_OWNED;
    if (unlock_count > lock_count)
        return lightweight() ? SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF : SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF;

    lock_count -= unlock_count;
    if (lock_count == 0) {
        // The owner is the caller, which waits on nothing, so it has no queue place to move
        (void)clear_owner();
        serve();
        // A hand over leaves the number of the other waiters as a lightweight mutex's wait count
        if (owner && lightweight())
            workarea.get(mem)->waitCount = static_cast<std::uint32_t>(waiters.size());
    }
    update_workarea(mem);

    return SCE_KERNEL_OK;
}

void Mutex::release_if_owned_by(ThreadState &thread) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted || owner_thread != &thread)
        return;
    lock_count = 0;
    // The thread end sets its priority itself once every mutex is given back
    (void)clear_owner();
    serve();
}

void Mutex::serve() {
    if (owner)
        return;
    if (ThreadState *const waiter = first_waiter()) {
        lock_count = waiter->wait_request().count;
        set_owner(*waiter);
        wake_waiter(*waiter);
    }
}

SceInt32 Mutex::cancel(ThreadState &thread, SceInt32 new_count, SceUInt32 *num_wait_threads) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID : SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;

    // Any negative count restores the lock count the mutex was created with
    if (new_count < 0)
        new_count = init_count;
    if (new_count > 1 && !(attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    // The owner is freed first, then the waiters, and a count left gives the mutex to the caller
    const ThreadStateRef freed = clear_owner();
    const auto nb_threads = static_cast<SceUInt32>(wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
    lock_count = new_count;
    if (new_count > 0)
        set_owner(thread);
    if (num_wait_threads)
        *num_wait_threads = nb_threads;
    // The freed owner runs at its new priority already, and moves in the queue of another object it
    // may wait on once the lock is let go
    guard.unlock();
    if (freed)
        freed->take_priority_place();

    return SCE_KERNEL_OK;
}

SceInt32 LwMutex::check_create(const char *name, SceUInt32 attr, SceInt32 init_count, const SceKernelLwMutexOptParam *opt) {
    if (attr & ~(SCE_KERNEL_MUTEX_ATTR_RECURSIVE | SCE_KERNEL_ATTR_TH_PRIO))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (opt && opt->size > sizeof(*opt))
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    if (init_count < 0 || (init_count > 1 && !(attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE)))
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (!name)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    return SCE_KERNEL_OK;
}

SceInt32 LwMutex::info(SceKernelLwMutexInfo &info) const {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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

void RWLock::end_waits(SceUID, std::unique_lock<std::mutex> &) {
    wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    clear_write_owner();
}

void RWLock::set_write_owner(ThreadState &thread) {
    if (write_owner == &thread)
        return;
    if (write_owner)
        write_owner->remove_held(*this);
    write_owner = &thread;
    thread.add_held(*this);
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
    const bool writer_waits = find_waiter([](auto &waiter) { return waiter.wait_request().write; }) != nullptr;
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
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
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
        const SceInt32 result = Waitable::wait(guard, thread, type, { .write = is_write }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return finish_wait(guard, thread, callbacks, result);
    }
}

SceInt32 RWLock::try_acquire(ThreadState &thread, bool is_write) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted || write_owner != &thread)
        return;
    lock_count = 0;
    clear_write_owner();
    serve();
}

SceInt32 RWLock::info(SceKernelRWLockInfo &info) const {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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
    auto *const writer = find_waiter([](auto &waiter) { return waiter.wait_request().write; });
    if (!writer) {
        lock_count += static_cast<SceInt32>(wake_all());
    } else if (lock_count == 0) {
        set_write_owner(*writer);
        lock_count = 1;
        wake_waiter(*writer);
    }
}

SceInt32 RWLock::cancel(ThreadState &thread, SceUInt32 *num_read_wait_threads, SceUInt32 *num_write_wait_threads, SceInt32 flag) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;

    const auto is_writer = [](auto &waiter) { return waiter.wait_request().write; };
    const auto nb_writers = static_cast<SceUInt32>(wake_all_if(is_writer, SCE_KERNEL_ERROR_WAIT_CANCEL));
    const auto nb_readers = static_cast<SceUInt32>(wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));

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

SceInt32 Semaphore::check_create(const char *name, SceUInt32 attr, SceInt32 init_val, SceInt32 max_val, const SceKernelSemaOptParam *opt) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (attr & ~(SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (init_val < 0 || max_val <= 0 || init_val > max_val)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (opt && opt->size > sizeof(*opt))
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    return SCE_KERNEL_OK;
}

void Semaphore::end_waits(SceUID, std::unique_lock<std::mutex> &) {
    wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
}

SceInt32 Semaphore::wait(ThreadState &thread, SceInt32 need_count, SceUInt32 *timeout, bool callbacks) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    if (need_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_SEMAPHORE, callbacks);
    const Deadline deadline = deadline_from(timeout);
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return r;
        if (try_take(thread, need_count))
            return SCE_KERNEL_OK;
        const SceInt32 result = Waitable::wait(guard, thread, type, { .count = need_count }, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return finish_wait(guard, thread, callbacks, result);
    }
}

SceInt32 Semaphore::poll(ThreadState &thread, SceInt32 need_count) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    if (need_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    return try_take(thread, need_count) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_SEMA_ZERO;
}

bool Semaphore::try_take(ThreadState &thread, SceInt32 need_count) {
    // A waiter queued ahead goes first, even one the count doesn't cover yet
    if (val < need_count || !first_for(thread.get_priority()))
        return false;
    val -= need_count;
    return true;
}

SceInt32 Semaphore::signal(SceInt32 count) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;

    if (count > max - val) {
        return SCE_KERNEL_ERROR_SEMA_OVF;
    }
    val += count;
    serve();

    return SCE_KERNEL_OK;
}

void Semaphore::serve() {
    while (ThreadState *const waiter = first_waiter()) {
        if (val < waiter->wait_request().count)
            break;
        val -= waiter->wait_request().count;
        wake_waiter(*waiter);
    }
}

SceInt32 Semaphore::cancel(SceInt32 set_count, SceUInt32 *num_wait_threads) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;

    if (set_count > max)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;

    const auto nb_threads = static_cast<SceUInt32>(wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));
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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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
    , associated_mutex(std::move(associated_mutex))
    , mutex_id(this->associated_mutex->uid) {}

Condvar::~Condvar() {
    // One that was never deleted leaves its mutex's list here
    associated_mutex->remove_condition(*this);
}

SceInt32 Condvar::check_create(const char *name, SceUInt32 attr) {
    if (!name)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (attr & ~SCE_KERNEL_ATTR_TH_PRIO)
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    return SCE_KERNEL_OK;
}

void Condvar::end_waits(SceUID, std::unique_lock<std::mutex> &) {
    // wait() maps this to the condition's own delete error
    wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    associated_mutex->remove_condition(*this);
}

void Condvar::end_mutex_waits() {
    const std::lock_guard<std::mutex> guard(mutex);
    // Deleted meanwhile, which ended its waits already
    if (deleted)
        return;
    mutex_id = -1;
    wake_all(lightweight() ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX : SCE_KERNEL_ERROR_WAIT_DELETE_MUTEX);
}

SceInt32 Condvar::wait(MemState &mem, ThreadState &thread, SceUInt32 *timeout, bool callbacks) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID : SCE_KERNEL_ERROR_UNKNOWN_COND_ID;

    // A deleted mutex gives the unknown mutex error
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
        r = Waitable::wait(guard, thread, type, {}, deadline, timeout);
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
    // A condition variable deleted after the signal or the timeout ends the wait without the mutex
    if (deleted)
        return r;

    guard.unlock();
    // Taking the mutex back is still part of the condition variable wait, a wait on the mutex with no
    // timeout that leaves the time left as it was at the signal
    const SceInt32 result = associated_mutex->relock(mem, thread, callbacks);
    if (timed_out && result == SCE_KERNEL_OK)
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    // A mutex deleted during the relock gives the mutex delete error, one deleted before it the
    // unknown mutex error
    if (result == SCE_KERNEL_ERROR_WAIT_DELETE)
        return lightweight() ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX : SCE_KERNEL_ERROR_WAIT_DELETE_MUTEX;
    // only heavy mutexes can be canceled
    if (result == SCE_KERNEL_ERROR_WAIT_CANCEL)
        return SCE_KERNEL_ERROR_WAIT_CANCEL_MUTEX;
    return result;
}

SceInt32 Condvar::signal(SignalTarget target) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return lightweight() ? SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID : SCE_KERNEL_ERROR_UNKNOWN_COND_ID;

    if (target.type == SignalTarget::Type::Specific) {
        // Search for specified waiting thread
        auto *waiter = find_waiter([&](auto &w) { return w.id == target.thread_id; });
        if (!waiter)
            return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
        wake_waiter(*waiter);
    } else if (target.type == SignalTarget::Type::Any) {
        // Wakes the first waiter in queue order
        if (ThreadState *const waiter = first_waiter())
            wake_waiter(*waiter);
    } else {
        wake_all();
    }

    return SCE_KERNEL_OK;
}

SceUInt32 Condvar::num_waiters() const {
    const std::lock_guard<std::mutex> guard(mutex);
    return deleted ? 0 : static_cast<SceUInt32>(waiters.size());
}

SceInt32 HeavyCond::info(SceKernelCondInfo &info) const {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_COND_ID;

    info = {};
    info.condId = uid;
    std::ranges::copy(name, info.name);
    info.attr = attr;
    info.mutexId = mutex_id;
    info.numWaitThreads = static_cast<SceUInt32>(waiters.size());
    return SCE_KERNEL_OK;
}

LwCond::LwCond(SceUInt32 attr, const char *name, MutexRef associated_mutex, Ptr<SceKernelLwCondWork> workarea)
    : WithUidClass(attr, name, std::move(associated_mutex))
    , workarea(workarea) {}

SceInt32 LwCond::info(SceKernelLwCondInfo &info) const {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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

SceInt32 EventFlag::check_create(const char *name, SceUInt32 attr, const SceKernelEventFlagOptParam *opt) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (attr & ~(SCE_EVENT_WAITMULTIPLE | SCE_KERNEL_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (opt && opt->size > sizeof(*opt))
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    return SCE_KERNEL_OK;
}

SceInt32 EventFlag::check_mode(SceUInt32 wait_mode) {
    if (wait_mode & ~(SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR | SCE_EVENT_WAITCLEAR_PAT))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    // Only one way of clearing the bits after the wait
    if ((wait_mode & SCE_EVENT_WAITCLEAR) && (wait_mode & SCE_EVENT_WAITCLEAR_PAT))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    return SCE_KERNEL_OK;
}

void EventFlag::end_waits(SceUID, std::unique_lock<std::mutex> &) {
    wake_all(SCE_KERNEL_ERROR_WAIT_DELETE);
    clear_holder();
    holds = 0;
}

void EventFlag::wait_ended(ThreadState &thread, SceInt32 result) {
    if (result == SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
        return;
    // A set's take() already wrote the bits from before its clear
    if (result != SCE_KERNEL_OK && thread.wait_request().out_pattern)
        *thread.wait_request().out_pattern = flags;
    // The holder is the waiter, since a second wait gets EVF_MULTI, and its lock is held here
    if (holds > 0 && --holds == 0) {
        thread.unlist_held(*this);
        holder = nullptr;
    }
}

void EventFlag::drop_hold() {
    if (holds > 0 && --holds == 0)
        clear_holder();
}

void EventFlag::set_holder(ThreadState &thread) {
    assert(!holder);
    holder = &thread;
    thread.add_held(*this);
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
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    // A wait checks the mode before the pattern, a poll the other way round
    if (const SceInt32 error = check_mode(wait_mode))
        return error;
    if (pattern == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    if (held_by_other(thread.id))
        return SCE_KERNEL_ERROR_EVF_MULTI;

    // A flag without SCE_EVENT_WAITMULTIPLE is held while the wait lasts, its own take or its end releases it
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
        if (take(request)) {
            drop_hold();
            break;
        }
        // A wait whose deadline is already gone returns these bits
        if (out_bits)
            *out_bits = flags;
        r = Waitable::wait(guard, thread, type, request, deadline, timeout);
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
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    if (pattern == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    if (const SceInt32 error = check_mode(wait_mode))
        return error;

    return try_take(thread_id, pattern, wait_mode, out_bits);
}

void EventFlag::release_if_held_by(ThreadState &thread) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted || holder != &thread)
        return;
    holds = 0;
    clear_holder();
}

SceInt32 EventFlag::set(SceUInt32 pattern) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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
    return true;
}

void EventFlag::serve() {
    wake_all_if([&](auto &waiter) { return take(waiter.wait_request()); });
}

SceInt32 EventFlag::clear(SceUInt32 pattern) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    flags &= pattern;

    return SCE_KERNEL_OK;
}

SceInt32 EventFlag::cancel(SceUInt32 pattern, SceUInt32 *num_wait_threads) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;

    // The waiters get the new bits as their out bits
    flags = pattern;
    const auto nb_threads = static_cast<SceUInt32>(wake_all(SCE_KERNEL_ERROR_WAIT_CANCEL));

    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

SceInt32 EventFlag::info(SceKernelEventFlagInfo &info) const {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
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

MsgPipe::MsgPipe(SceUInt32 attr, const char *name, SceSize buf_size, SceUID creator)
    : WithUidClass(attr, name, 0, system_event_user_data(creator))
    // A pipe's buffer is a whole number of 4 KiB pages
    , capacity(align(buf_size, 0x1000))
    , buffer(std::make_unique<std::byte[]>(capacity)) {}

SceInt32 MsgPipe::check_create(const char *name, SceUInt32 type, SceUInt32 attr, SceSize buf_size) {
    if (const SceInt32 error = check_name(name, attr))
        return error;
    if (type != SCE_KERNEL_MSG_PIPE_TYPE_USER_MAIN)
        return SCE_KERNEL_ERROR_ILLEGAL_TYPE;
    if (attr & ~(SCE_KERNEL_MSG_PIPE_ATTR_TH_MASK | SCE_KERNEL_ATTR_OPENABLE | SCE_KERNEL_EVENT_ATTR_AUTO_RESET | SCE_KERNEL_ATTR_NOTIFY_CB_WAKEUP_ONLY))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    // A size that rounds to no page fails, also one that wraps past 4 GiB
    if (static_cast<SceSize>(buf_size + 0xFFF) >> 12 == 0)
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    return SCE_KERNEL_OK;
}

void MsgPipe::end_waits(SceUID deleter, std::unique_lock<std::mutex> &guard) {
    write_pos = 0;
    read_pos = 0;
    held = 0;
    waiting_side = Side::none;
    side_count = 0;
    head_partial = false;
    Event::end_waits(deleter, guard);
}

void MsgPipe::waiter_added(ThreadState &thread) {
    const WaitRequest &request = thread.wait_request();
    if (!request.sender && !request.receiver)
        return;
    if (goes_first(request, thread.queued_priority))
        head_partial = request.sender ? request.sender->keeps_place() : request.receiver->keeps_place();
    const bool priority = by_priority(attr, request);
    attr = (attr & ~SCE_KERNEL_ATTR_TH_PRIO) | (priority ? SCE_KERNEL_ATTR_TH_PRIO : 0);
    waiting_side = request.sender ? Side::senders : Side::receivers;
    ++side_count;
}

bool MsgPipe::queues_behind_head(const ThreadState &thread) const {
    // A head that moved part of its transfer keeps the first place against any send or receive
    const WaitRequest &request = thread.wait_request();
    return (request.sender || request.receiver) && head_partial;
}

void MsgPipe::wait_ended(ThreadState &thread, SceInt32) {
    const WaitRequest &request = thread.wait_request();
    if ((!request.sender && !request.receiver) || waiting_side == Side::none)
        return;
    head_partial = false;
    if (waiting_side == Side::senders) {
        if (const ThreadState *head = first_sender())
            head_partial = head->wait_request().sender->keeps_place();
    } else if (ThreadState *head = first_receiver()) {
        MsgPipeReceiver &receiver = *head->wait_request().receiver;
        // A peek puts back the ring
        const SceSize read_was = read_pos, held_was = held;
        copy_ring_to_pieces(receiver, std::min(receiver.left, held));
        if (receiver.peek) {
            read_pos = read_was;
            held = held_was;
        }
        receiver.report();
        head_partial = receiver.left != 0 && receiver.keeps_place();
    }
    if (write_pos == read_pos && held == 0)
        pattern &= ~SCE_KERNEL_EVENT_DATA_EXIST;
    if (--side_count == 0)
        waiting_side = Side::none;
}

SceInt32 MsgPipe::transfer_result(std::unique_lock<std::mutex> &guard, SceInt32 result, const MsgPipeTransfer &transfer, bool sending) {
    if (result == SCE_KERNEL_ERROR_THREAD_STOPPED)
        return result;
    const SceUInt32 moved = transfer.total - transfer.left;
    SceUInt32 bits = moved ? (sending ? SCE_KERNEL_EVENT_IN : SCE_KERNEL_EVENT_OUT) : 0;
    if (result < 0)
        bits |= SCE_KERNEL_EVENT_ERROR;
    if (sending && moved && held != 0)
        bits |= SCE_KERNEL_EVENT_DATA_EXIST;
    if (!sending && held == 0)
        pattern &= ~SCE_KERNEL_EVENT_DATA_EXIST;
    if (bits) {
        const SceUInt64 data = static_cast<SceUInt32>(result)
            | (static_cast<SceUInt64>(result < 0 ? ~0U : moved) << 32);
        fire(bits, data, true);
        notify_event_callbacks(guard, bits);
    }
    return result;
}

std::expected<SceSize, SceInt32> MsgPipe::check_pieces(std::span<const MsgPipeVector> pieces) const {
    // Each piece is read once, so the total the transfer uses is the one checked here
    uint64_t total = 0;
    for (const MsgPipeVector &piece : pieces) {
        // Even an empty piece needs a buffer
        if (!piece.base)
            return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
        total += piece.size;
        if (total > capacity)
            return std::unexpected(SCE_KERNEL_ERROR_ILLEGAL_SIZE);
    }
    return static_cast<SceSize>(total);
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

std::span<std::byte> MsgPipeTransfer::run(SceSize n) {
    // Steps over the pieces already moved, and empty ones
    while (offset == pieces[piece].size) {
        ++piece;
        offset = 0;
    }
    const MsgPipeVector &current = pieces[piece];
    return std::span{ current.base.cast<std::byte>().get(*mem), current.size }.subspan(offset, std::min(n, current.size - offset));
}

void MsgPipeTransfer::advance(SceSize n) {
    offset += n;
    left -= n;
}

void MsgPipeTransfer::report() const {
    if (size)
        *size = total - left;
}

void MsgPipe::copy_pieces_to_ring(MsgPipeSender &sender, SceSize n, bool commit) {
    // Each copy ends at a piece end or at the ring end
    MsgPipeSender from = sender;
    while (n > 0) {
        const std::span<std::byte> in = from.run(std::min(n, capacity - write_pos));
        std::ranges::copy(in, buffer.get() + write_pos);
        const SceSize moved = static_cast<SceSize>(in.size());
        write_pos = (write_pos + moved) % capacity;
        held += moved;
        from.advance(moved);
        n -= moved;
    }
    if (commit)
        sender = from;
}

void MsgPipe::copy_ring_to_pieces(MsgPipeReceiver &receiver, SceSize n) {
    while (n > 0) {
        const std::span<std::byte> out = receiver.run(std::min(n, capacity - read_pos));
        std::ranges::copy_n(buffer.get() + read_pos, out.size(), out.begin());
        const SceSize moved = static_cast<SceSize>(out.size());
        read_pos = (read_pos + moved) % capacity;
        held -= moved;
        receiver.advance(moved);
        n -= moved;
    }
}

void MsgPipe::fill(MsgPipeSender &sender) {
    // ASAP puts what fits, FULL all of its rest or nothing
    const SceSize n = std::min(sender.left, capacity - held);
    if (sender.full && n < sender.left)
        return;
    copy_pieces_to_ring(sender, n, true);
}

SceSize MsgPipe::available() const {
    SceSize bytes = held;
    for (const ThreadState &waiter : waiters) {
        if (const MsgPipeSender *const sender = waiter.wait_request().sender)
            bytes += sender->left;
    }
    return bytes;
}

ThreadState *MsgPipe::first_sender() {
    for (ThreadState &waiter : waiters) {
        if (waiter.wait_request().sender)
            return &waiter;
    }
    return nullptr;
}

ThreadState *MsgPipe::first_receiver() {
    for (ThreadState &waiter : waiters) {
        if (waiter.wait_request().receiver)
            return &waiter;
    }
    return nullptr;
}

bool MsgPipe::give(MsgPipeSender &sender) {
    // A send reaches the first waiting receiver even with no bytes, which ends an ASAP receive
    while (ThreadState *const receiver = first_receiver()) {
        MsgPipeReceiver &to = *receiver->wait_request().receiver;
        const SceSize n = std::min(to.left, sender.left);
        // The bytes go through the ring. A peek goes through a ring that looks empty at its write
        // position, then gets back its read position and count but keeps the new write position.
        const SceSize read_was = read_pos, held_was = held;
        if (to.peek) {
            read_pos = write_pos;
            held = 0;
        }
        copy_pieces_to_ring(sender, n, !to.peek);
        copy_ring_to_pieces(to, n);
        if (to.peek) {
            read_pos = read_was;
            held = held_was;
        }
        to.report();
        // A FULL receive that still wants bytes stays the first one
        if (to.full && to.left > 0) {
            head_partial = to.keeps_place();
            break;
        }
        head_partial = false;
        wake_waiter(*receiver);
        if (sender.left == 0)
            break;
    }
    fill(sender);
    sender.report();
    return sender.done();
}

bool MsgPipe::take(MsgPipeReceiver &receiver) {
    // A peek puts back the ring at the end
    const SceSize write_was = write_pos, read_was = read_pos, held_was = held;
    copy_ring_to_pieces(receiver, std::min(receiver.left, held));
    // Each waiting sender gives its bytes to the receiver through the ring while it wants some, then
    // puts its rest into the pipe, up to the first sender that stays
    while (ThreadState *const sender = first_sender()) {
        MsgPipeSender &from = *sender->wait_request().sender;
        const SceSize n = std::min(receiver.left, from.left);
        copy_pieces_to_ring(from, n, !receiver.peek);
        copy_ring_to_pieces(receiver, n);
        // A peek frees nothing
        if (receiver.peek)
            break;
        fill(from);
        from.report();
        if (!from.done()) {
            head_partial = from.keeps_place();
            break;
        }
        head_partial = false;
        wake_waiter(*sender);
    }
    if (receiver.peek) {
        write_pos = write_was;
        read_pos = read_was;
        held = held_was;
    }
    receiver.report();
    return receiver.done();
}

void MsgPipe::serve() {
    if (waiting_side != Side::senders)
        return;
    for (auto it = waiters.begin(); it != waiters.end();) {
        ThreadState &waiter = *it++;
        MsgPipeSender *const sender = waiter.wait_request().sender;
        if (!sender)
            continue;
        fill(*sender);
        sender->report();
        if (!sender->done())
            return;
        head_partial = false;
        wake_waiter(waiter);
    }
}

bool MsgPipe::goes_first(const WaitRequest &request, int priority) {
    const ThreadState *const head = request.sender ? first_sender() : first_receiver();
    if (!head)
        return true;
    // Lower value is higher priority, and nothing goes ahead of a head that moved part of its bytes
    return by_priority(attr, request) && head->queued_priority > priority && !head_partial;
}

SceInt32 MsgPipe::receive(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks) {
    // DONT_WAIT ends like a try, and a wait with callbacks runs them after it
    if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT) {
        const SceInt32 result = try_receive(thread, mem, wait_mode, pieces, size);
        if (callbacks)
            thread.process_callbacks();
        return result;
    }
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    const std::expected<SceSize, SceInt32> total = check_pieces(pieces);
    if (!total)
        return total.error();
    MsgPipeReceiver receiver(mem, pieces, size, *total, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE);
    // A receive of nothing ends at once, before any callback runs
    if (receiver.total == 0) {
        receiver.report();
        return SCE_KERNEL_OK;
    }

    const WaitRequest request{ .receiver = &receiver };
    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_MSG_PIPE, callbacks);
    const Deadline deadline = deadline_from(timeout);
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return transfer_result(guard, r, receiver, false);
        // Don't take anything while other receivers are ahead in the queue. The size is written
        // after every attempt.
        const bool done = goes_first(request, thread.get_priority()) && take(receiver);
        receiver.report();
        // The callbacks notified meanwhile run also after a receive that got its bytes
        if (done)
            return finish_wait(guard, thread, callbacks, transfer_result(guard, SCE_KERNEL_OK, receiver, false));
        // A receive that took the last bytes and waits for more leaves no data to report
        if (held == 0)
            pattern &= ~SCE_KERNEL_EVENT_DATA_EXIST;
        const SceInt32 result = Waitable::wait(guard, thread, type, request, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return finish_wait(guard, thread, callbacks, transfer_result(guard, result, receiver, false));
    }
}

SceInt32 MsgPipe::send(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size, SceUInt32 *timeout, bool callbacks) {
    // DONT_WAIT ends like a try, and a wait with callbacks runs them after it
    if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT) {
        const SceInt32 result = try_send(thread, mem, wait_mode, pieces, size);
        if (callbacks)
            thread.process_callbacks();
        return result;
    }
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    const std::expected<SceSize, SceInt32> total = check_pieces(pieces);
    if (!total)
        return total.error();
    MsgPipeSender sender(mem, pieces, size, *total, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);
    // A send of nothing ends at once, before any callback runs
    if (sender.total == 0) {
        sender.report();
        return SCE_KERNEL_OK;
    }

    const WaitRequest request{ .sender = &sender };
    const SceUInt32 type = wait_type(SCE_KERNEL_WAITTYPE_MSG_PIPE, callbacks);
    const Deadline deadline = deadline_from(timeout);
    while (true) {
        if (const SceInt32 r = run_wait_callbacks(guard, thread, callbacks))
            return transfer_result(guard, r, sender, true);
        // Don't send anything while other senders are ahead in the queue. The size is written
        // after every attempt.
        const bool done = goes_first(request, thread.get_priority()) && give(sender);
        sender.report();
        // The callbacks notified meanwhile run also after a send that moved its bytes
        if (done)
            return finish_wait(guard, thread, callbacks, transfer_result(guard, SCE_KERNEL_OK, sender, true));
        const SceInt32 result = Waitable::wait(guard, thread, type, request, deadline, timeout);
        if (result != SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
            return finish_wait(guard, thread, callbacks, transfer_result(guard, result, sender, true));
    }
}

SceInt32 MsgPipe::try_receive(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    const std::expected<SceSize, SceInt32> total = check_pieces(pieces);
    if (!total)
        return total.error();
    MsgPipeReceiver receiver(mem, pieces, size, *total, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE);
    // Behind the receivers already waiting, or short of bytes for all of a FULL receive, it fails
    // without writing a size
    if (!goes_first({ .receiver = &receiver }, thread.get_priority()) || (receiver.full && receiver.total > available()))
        return transfer_result(guard, SCE_KERNEL_ERROR_MSG_PIPE_EMPTY, receiver, false);
    return transfer_result(guard, take(receiver) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_MSG_PIPE_EMPTY, receiver, false);
}

SceInt32 MsgPipe::try_send(ThreadState &thread, MemState &mem, SceUInt32 wait_mode, std::span<const MsgPipeVector> pieces, SceSize *size) {
    std::unique_lock<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;
    const std::expected<SceSize, SceInt32> total = check_pieces(pieces);
    if (!total)
        return total.error();
    MsgPipeSender sender(mem, pieces, size, *total, wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);
    // Behind the senders already waiting, or short of room for all of a FULL send, it fails without
    // writing a size
    if (!goes_first({ .sender = &sender }, thread.get_priority()) || (sender.full && sender.total > capacity - held))
        return transfer_result(guard, SCE_KERNEL_ERROR_MSG_PIPE_FULL, sender, true);
    return transfer_result(guard, give(sender) ? SCE_KERNEL_OK : SCE_KERNEL_ERROR_MSG_PIPE_FULL, sender, true);
}

SceInt32 MsgPipe::cancel(SceUInt32 *num_send_wait_threads, SceUInt32 *num_receive_wait_threads) {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;

    const SceUInt32 senders = waiting_side == Side::senders ? side_count : 0;
    const SceUInt32 receivers = waiting_side == Side::receivers ? side_count : 0;
    wake_all_if([](ThreadState &waiter) {
        const WaitRequest &request = waiter.wait_request();
        return request.sender || request.receiver;
    },
        SCE_KERNEL_ERROR_WAIT_CANCEL);
    write_pos = 0;
    read_pos = 0;
    held = 0;
    pattern &= ~SCE_KERNEL_EVENT_DATA_EXIST;

    if (num_send_wait_threads)
        *num_send_wait_threads = senders;
    if (num_receive_wait_threads)
        *num_receive_wait_threads = receivers;

    return SCE_KERNEL_OK;
}

SceInt32 MsgPipe::info(SceKernelMppInfo &info) const {
    const std::lock_guard<std::mutex> guard(mutex);
    if (deleted)
        return SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID;

    info = {};
    info.mppId = uid;
    std::ranges::copy(name, info.name);
    info.attr = attr;
    info.bufSize = static_cast<SceInt32>(capacity);
    info.freeSize = static_cast<SceInt32>(capacity - held);
    info.numSendWaitThreads = waiting_side == Side::senders ? static_cast<SceInt32>(waiters.size()) : 0;
    info.numReceiveWaitThreads = waiting_side == Side::receivers ? static_cast<SceInt32>(waiters.size()) : 0;
    return SCE_KERNEL_OK;
}
