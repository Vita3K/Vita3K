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
#include <kernel/callback.h>
#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <kernel/thread/wait_functions.h>

#include <kernel/types.h>
#include <util/lock_and_find.h>
#include <util/log.h>

static constexpr bool LOG_SYNC_PRIMITIVES = false;

// ***********
// * Helpers *
// ***********

inline static int unknown_mutex_id(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID);
    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
}

inline static int unknown_cond_id(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID);
    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
}

inline static int mutex_recursive_error(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE);
    return RET_ERROR(SCE_KERNEL_ERROR_MUTEX_RECURSIVE);
}

inline static int mutex_lock_ovf_error(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF);
    return RET_ERROR(SCE_KERNEL_ERROR_MUTEX_LOCK_OVF);
}

inline static int mutex_unlock_udf_error(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF);
    return RET_ERROR(SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF);
}

inline static int mutex_failed_to_own_error(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN);
    return RET_ERROR(SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN);
}

inline static int mutex_not_owned_error(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED);
    return RET_ERROR(SCE_KERNEL_ERROR_MUTEX_NOT_OWNED);
}

static int map_wait_end_reason(const WaitEndReason end_reason, const int canceled_error, const int deleted_error = SCE_KERNEL_ERROR_WAIT_DELETE) {
    switch (end_reason) {
    case WaitEndReason::Signaled:
        return SCE_KERNEL_OK;
    case WaitEndReason::Deleted:
        return deleted_error;
    case WaitEndReason::Canceled:
    case WaitEndReason::None:
        return canceled_error;
    case WaitEndReason::Timeout:
        return static_cast<int>(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    }

    return canceled_error;
}

template <typename T>
static T *get_workarea_ptr(const Ptr<T> workarea_handle, MemState &mem) {
    if (!workarea_handle)
        return nullptr;

    return workarea_handle.get(mem);
}

static void sync_lwmutex_workarea(Mutex &mutex, MemState &mem) {
    auto *workarea = get_workarea_ptr(mutex.workarea, mem);
    if (!workarea)
        return;

    workarea->uid = mutex.uid;
    workarea->owner = 0;
    workarea->lockCount = mutex.lock_count;
    if (workarea->lockCount)
        workarea->owner = mutex.owner;
    workarea->attr = mutex.attr;
}

template <typename T>
static void invalidate_workarea(const Ptr<T> workarea_handle, MemState &mem) {
    auto *workarea = get_workarea_ptr(workarea_handle, mem);
    if (!workarea)
        return;

    memset(workarea, 0, sizeof(*workarea));
    workarea->uid = SCE_UID_INVALID_UID;
}

static bool queue_uses_priority(const uint32_t attr) {
    return (attr & SCE_KERNEL_ATTR_TH_PRIO) != 0;
}

static WaiterRef make_waiter_ref(const ThreadState &thread) {
    return WaiterRef{
        .thread_id = thread.id,
        .wait_sequence = thread.wait_sequence,
        .priority = thread.priority,
    };
}

static void enqueue_waiter(WaitQueue &queue, const WaiterRef &ref, const bool priority_order) {
    if (!priority_order) {
        queue.push_back(ref);
        return;
    }

    const auto it = std::find_if(queue.begin(), queue.end(), [&](const WaiterRef &queued) {
        return queued.priority > ref.priority;
    });
    queue.insert(it, ref);
}

static auto find_waiter(WaitQueue &queue, const SceUID thread_id, const uint64_t wait_sequence) {
    return std::find_if(queue.begin(), queue.end(), [&](const WaiterRef &ref) {
        return ref.thread_id == thread_id && ref.wait_sequence == wait_sequence;
    });
}

static auto find_waiter_by_thread(WaitQueue &queue, const SceUID thread_id) {
    return std::find_if(queue.begin(), queue.end(), [&](const WaiterRef &ref) {
        return ref.thread_id == thread_id;
    });
}

static bool erase_waiter_if_present(WaitQueue &queue, const SceUID thread_id, const uint64_t wait_sequence) {
    const auto it = find_waiter(queue, thread_id, wait_sequence);
    if (it == queue.end())
        return false;

    queue.erase(it);
    return true;
}

static ThreadStatePtr get_waiting_thread(KernelState &kernel, const WaiterRef &ref, const WaitType expected_type, const SceUID expected_object);

static uint32_t wake_waiters_for_delete(KernelState &kernel, WaitQueue &queue, const WaitType expected_type, const SceUID object_id) {
    uint32_t waiter_count = 0;
    while (!queue.empty()) {
        const auto waiter = queue.front();
        if (const ThreadStatePtr waiting_thread = get_waiting_thread(kernel, waiter, expected_type, object_id))
            kernel.thread_manager.cancel_wait(*waiting_thread, waiter.wait_sequence, WaitEndReason::Deleted);

        queue.pop_front();
        ++waiter_count;
    }

    return waiter_count;
}

static void notify_msgpipe_delete_waiter(MsgPipe &msgpipe) {
    const std::size_t remaining = msgpipe.remainingThreads.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (remaining == 0)
        msgpipe.delete_cond.notify_all();
}

static void write_wait_u32(MemState &mem, const Address addr, const uint32_t value) {
    if (addr != 0)
        *Ptr<uint32_t>(addr).get(mem) = value;
}

static void write_wait_u64(MemState &mem, const Address addr, const uint64_t value) {
    if (addr != 0)
        *Ptr<uint64_t>(addr).get(mem) = value;
}

struct WaitingThreadSnapshot {
    ThreadStatePtr thread;
    WaitState wait;
};

static bool get_waiting_thread_snapshot(KernelState &kernel, const WaiterRef &ref, const WaitType expected_type, const SceUID expected_object, WaitingThreadSnapshot &snapshot) {
    const ThreadStatePtr thread = kernel.get_thread(ref.thread_id);
    if (!thread)
        return false;

    std::lock_guard<std::mutex> thread_lock(thread->mutex);
    const WaitState *wait_state = thread->find_wait_state_by_sequence(ref.wait_sequence);
    if (!wait_state)
        return false;
    if (wait_state->type != expected_type)
        return false;
    if (expected_object != SCE_UID_INVALID_UID && wait_state->object_id != expected_object)
        return false;

    snapshot.thread = thread;
    snapshot.wait = *wait_state;
    return true;
}

static ThreadStatePtr get_waiting_thread(KernelState &kernel, const WaiterRef &ref, const WaitType expected_type, const SceUID expected_object) {
    WaitingThreadSnapshot snapshot;
    if (!get_waiting_thread_snapshot(kernel, ref, expected_type, expected_object, snapshot))
        return nullptr;

    return snapshot.thread;
}

template <typename ReadyFn>
static bool wake_first_matching_waiter(KernelState &kernel, WaitQueue &queue, const WaitType expected_type, const SceUID object_id, ReadyFn &&is_ready) {
    for (auto it = queue.begin(); it != queue.end();) {
        const auto waiter = *it;
        WaitingThreadSnapshot snapshot;
        if (!get_waiting_thread_snapshot(kernel, waiter, expected_type, object_id, snapshot)) {
            it = queue.erase(it);
            continue;
        }

        if (!is_ready(snapshot)) {
            ++it;
            continue;
        }

        WaitState::WaitCompletion completion;
        completion.condition_met = true;
        kernel.thread_manager.complete_wait(*snapshot.thread, waiter.wait_sequence, completion);
        queue.erase(it);
        return true;
    }

    return false;
}

static bool rwlock_is_write_locked(const RWLock &rwlock) {
    return rwlock.write_owner != SCE_UID_INVALID_UID;
}

static bool rwlock_is_read_locked(const RWLock &rwlock) {
    return rwlock.read_lock_count > 0;
}

static bool rwlock_is_unlocked(const RWLock &rwlock) {
    return !rwlock_is_write_locked(rwlock) && !rwlock_is_read_locked(rwlock);
}

static bool rwlock_has_waiting_writer(KernelState &kernel, const RWLock &rwlock) {
    for (const auto &waiter : rwlock.waiting_threads) {
        WaitingThreadSnapshot snapshot;
        if (get_waiting_thread_snapshot(kernel, waiter, WaitType::RWLock, rwlock.uid, snapshot) && snapshot.wait.payload.is_write)
            return true;
    }

    return false;
}

static std::size_t rwlock_count_waiters(KernelState &kernel, const RWLock &rwlock, const bool is_write) {
    std::size_t count = 0;
    for (const auto &waiter : rwlock.waiting_threads) {
        WaitingThreadSnapshot snapshot;
        if (get_waiting_thread_snapshot(kernel, waiter, WaitType::RWLock, rwlock.uid, snapshot) && snapshot.wait.payload.is_write == is_write)
            ++count;
    }

    return count;
}

static bool rwlock_can_grant_read(KernelState &kernel, const RWLock &rwlock, const ThreadStatePtr &thread) {
    if (rwlock_is_write_locked(rwlock))
        return false;

    if (rwlock.read_owners.contains(thread->id))
        return true;

    return !rwlock_has_waiting_writer(kernel, rwlock);
}

static bool rwlock_can_grant_write(const RWLock &rwlock) {
    return !rwlock_is_write_locked(rwlock) && !rwlock_is_read_locked(rwlock);
}

static bool semaphore_waits_for_queue(const Semaphore &semaphore, const int need_count, const int thread_priority) {
    if (semaphore.val < need_count || semaphore.waiting_threads.empty())
        return false;

    if ((semaphore.attr & SCE_KERNEL_ATTR_TH_PRIO) == 0)
        return true;

    const auto &first_waiter = semaphore.waiting_threads.front();
    return first_waiter.priority <= thread_priority;
}

static void cancel_timer_event(KernelState &kernel, Timer &timer) {
    if (timer.event_handle == 0)
        return;

    kernel.core_timing.cancel(timer.event_handle);
    timer.event_handle = 0;
}

static void arm_timer_event(KernelState &kernel, Timer &timer) {
    cancel_timer_event(kernel, timer);

    if (!timer.is_started || timer.event_interval == 0 || timer.next_event == std::numeric_limits<uint64_t>::max())
        return;

    const SceUID timer_uid = timer.uid;
    const uint64_t deadline_us = timer.next_event;
    timer.event_handle = kernel.core_timing.schedule_at(deadline_us,
        [&kernel, timer_uid](const CoreTimingEventContext &ctx) {
            const TimerPtr timer = lock_and_find(timer_uid, kernel.timers, kernel.mutex);
            if (!timer)
                return;

            std::lock_guard<std::mutex> lock(timer->mutex);
            if (!timer->is_started || timer->event_handle != ctx.handle || timer->next_event == std::numeric_limits<uint64_t>::max())
                return;

            const uint64_t fired_time = kernel.core_timing.now_us();
            if (!timer->is_pulse)
                timer->event_set = true;

            for (auto it = timer->waiting_threads.begin(); it != timer->waiting_threads.end();) {
                const auto waiter = *it;
                WaitingThreadSnapshot snapshot;
                if (!get_waiting_thread_snapshot(kernel, waiter, WaitType::Timer, timer->uid, snapshot)) {
                    it = timer->waiting_threads.erase(it);
                    continue;
                }

                if ((snapshot.wait.payload.pattern & SCE_KERNEL_EVENT_TIMER) == 0) {
                    ++it;
                    continue;
                }

                WaitState::WaitCompletion completion;
                completion.value_u32 = SCE_KERNEL_EVENT_TIMER;
                completion.value_u64 = 0;
                completion.condition_met = true;
                kernel.thread_manager.complete_wait(*snapshot.thread, waiter.wait_sequence, completion);
                timer->waiting_threads.erase(it);
                break;
            }

            if (timer->is_repeat) {
                timer->next_event += ((fired_time - timer->next_event) / timer->event_interval + 1) * timer->event_interval;
                arm_timer_event(kernel, *timer);
            } else {
                timer->next_event = std::numeric_limits<uint64_t>::max();
                timer->event_handle = 0;
            }
        });
}

inline static MutexPtrs &get_mutexes(KernelState &kernel, SyncWeight weight) {
    return weight == SyncWeight::Light ? kernel.lwmutexes : kernel.mutexes;
}

inline static CondvarPtrs &get_condvars(KernelState &kernel, SyncWeight weight) {
    return weight == SyncWeight::Light ? kernel.lwcondvars : kernel.condvars;
}

// *****************
// * Simple events *
// *****************

SceUID simple_event_create(KernelState &kernel, MemState &mem, const char *export_name, const char *name, SceUID thread_id, SceUInt32 attr, SceUInt32 init_pattern) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const SceUID uid = kernel.get_next_uid();

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} pattern: {:#b}",
            export_name, uid, thread_id, name, attr, init_pattern);
    }

    const SimpleEventPtr event = std::make_shared<SimpleEvent>();
    event->uid = uid;
    event->pattern = init_pattern;
    strncpy(event->name, name, KERNELOBJECT_MAX_NAME_LENGTH);
    event->attr = attr;
    event->last_user_data = 0;
    event->auto_reset = (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET);
    event->cb_wakeup_only = (event->attr & SCE_KERNEL_ATTR_NOTIFY_CB_WAKEUP_ONLY);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.simple_events.emplace(uid, event);

    return uid;
}

SceInt32 simple_event_waitorpoll(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID event_id, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, const Address result_pattern_addr, const Address user_data_addr, SceUInt32 *timeout, bool is_wait, bool callbacks_allowed) {
    if (is_wait)
        dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);

    const SimpleEventPtr event = lock_and_find(event_id, kernel.simple_events, kernel.mutex);
    if (!event) {
        // this may also be a timer event
        return timer_waitorpoll(kernel, mem, export_name, thread_id, event_id, wait_pattern, result_pattern, user_data, result_pattern_addr, user_data_addr, timeout, is_wait, callbacks_allowed);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_pattern: {:#b} wait_pattern: {:#b} timeout: {}"
                  " waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->pattern, wait_pattern, timeout ? *timeout : 0,
            event->waiting_threads.size());
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);

    std::unique_lock<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);

    if (result_pattern)
        *result_pattern = event->pattern;

    if (event->pattern & wait_pattern) {
        if (event->auto_reset)
            // all common bits are zeroed
            event->pattern &= ~wait_pattern;

        if (user_data)
            *user_data = event->last_user_data;

        return SCE_KERNEL_OK;
    } else if (is_wait) {
        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        auto wait_state = WaitState::simple_event(event->uid, wait_pattern, timeout ? *timeout : 0, timeout != nullptr, callbacks_allowed);
        wait_state.continuation.writeback.out_u32 = result_pattern_addr;
        wait_state.continuation.writeback.out_u64 = user_data_addr;
        kernel.thread_manager.begin_wait_locked(*thread, wait_state);
        const auto waiter = make_waiter_ref(*thread);
        enqueue_waiter(event->waiting_threads, waiter, queue_uses_priority(event->attr));
        event_lock.unlock();

        WaitEndReason end_reason = WaitEndReason::None;
        if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::SimpleEvent, event->uid, timeout)) {
            kernel.thread_manager.clear_wait_locked(*thread);
            thread_lock.unlock();
            event_lock.lock();
            erase_waiter_if_present(event->waiting_threads, waiter.thread_id, waiter.wait_sequence);

            if (user_data)
                *user_data = event->last_user_data;
            if (result_pattern)
                *result_pattern = event->pattern;

            return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
        }

        end_reason = thread->wait.end_reason;
        const auto wait_state_after = thread->wait;
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();

        if (end_reason == WaitEndReason::Signaled) {
            write_wait_u32(mem, wait_state_after.continuation.writeback.out_u32, wait_state_after.completion.value_u32);
            write_wait_u64(mem, wait_state_after.continuation.writeback.out_u64, wait_state_after.completion.value_u64);
        }

        return map_wait_end_reason(end_reason, SCE_KERNEL_ERROR_WAIT_CANCEL);
    } else {
        return SCE_KERNEL_ERROR_EVENT_COND;
    }
}

SceInt32 simple_event_setorpulse(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id, SceUInt32 pattern, SceUInt64 user_data, bool is_set) {
    const SimpleEventPtr event = lock_and_find(event_id, kernel.simple_events, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_pattern: {:#b} set_pattern: {:#b}"
                  " waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->pattern, pattern,
            event->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);

    const SceUInt32 old_pattern = event->pattern;
    const SceUInt64 old_user_data = event->last_user_data;
    const SceUInt32 new_pattern = event->pattern | pattern;

    event->pattern = new_pattern;
    event->last_user_data = user_data;

    for (auto it = event->waiting_threads.begin(); it != event->waiting_threads.end();) {
        const auto waiter = *it;
        WaitingThreadSnapshot snapshot;
        if (!get_waiting_thread_snapshot(kernel, waiter, WaitType::SimpleEvent, event->uid, snapshot)) {
            it = event->waiting_threads.erase(it);
            continue;
        }

        const auto waiting_pattern = snapshot.wait.payload.pattern;

        if (event->pattern & waiting_pattern) {
            if (event->auto_reset)
                // all common bit are zeroed
                event->pattern &= ~waiting_pattern;

            WaitState::WaitCompletion completion;
            completion.value_u32 = new_pattern;
            completion.value_u64 = event->last_user_data;
            completion.condition_met = true;
            kernel.thread_manager.complete_wait(*snapshot.thread, waiter.wait_sequence, completion);

            it = event->waiting_threads.erase(it);
        } else {
            ++it;
        }
    }

    if (!is_set) {
        event->pattern = old_pattern;
        event->last_user_data = old_user_data;
    }

    return SCE_KERNEL_OK;
}

SceInt32 simple_event_clear(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id, SceUInt32 clear_pattern) {
    const SimpleEventPtr event = lock_and_find(event_id, kernel.simple_events, kernel.mutex);
    if (!event) {
        // this may also be a timer event
        return timer_clear(kernel, export_name, thread_id, event_id, clear_pattern);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} clear_pattern: {:#b}",
            export_name, event_id, thread_id, clear_pattern);
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);

    event->pattern &= clear_pattern;

    return SCE_KERNEL_OK;
}

SceInt32 simple_event_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id) {
    const SimpleEventPtr event = lock_and_find(event_id, kernel.simple_events, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_pattern: {:#b} waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->pattern, event->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);

    event->deleted = true;
    wake_waiters_for_delete(kernel, event->waiting_threads, WaitType::SimpleEvent, event->uid);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.simple_events.erase(event_id);

    return SCE_KERNEL_OK;
}

// *********
// * Timer *
// *********

SceUID timer_create(KernelState &kernel, MemState &mem, const char *export_name, const char *name, SceUID thread_id, SceUInt32 attr) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const SceUID uid = kernel.get_next_uid();

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {}",
            export_name, uid, thread_id, name, attr);
    }

    const TimerPtr timer = std::make_shared<Timer>();
    timer->uid = uid;
    timer->next_event = std::numeric_limits<uint64_t>::max();
    strncpy(timer->name, name, KERNELOBJECT_MAX_NAME_LENGTH);
    timer->attr = attr;
    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.timers.emplace(uid, timer);

    return uid;
}

SceUID timer_find(KernelState &kernel, const char *export_name, const char *pName) {
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    if (LOG_SYNC_PRIMITIVES)
        LOG_DEBUG("{}: name: \"{}\"", export_name, pName);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);

    const auto it = std::find_if(kernel.timers.begin(), kernel.timers.end(), [=](const auto &timer) {
        return strncmp(timer.second->name, pName, KERNELOBJECT_MAX_NAME_LENGTH) == 0;
    });

    if (it != kernel.timers.end())
        return it->first;

    return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);
}

static void timer_schedule_event(KernelState &kernel, Timer &timer) {
    const uint64_t curr_time = kernel.core_timing.now_us();
    timer.next_event = curr_time + timer.event_interval;
    arm_timer_event(kernel, timer);
}

SceInt32 timer_set(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID timer_handle, SceUID type, SceKernelSysClock *interval, SceInt32 repeats) {
    TimerPtr timer = lock_and_find(timer_handle, kernel.timers, kernel.mutex);
    if (!timer)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    if (!interval)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} type: {} interval: {} repeats: {}"
                  " waiting_threads: {}",
            export_name, timer->uid, thread_id, timer->name, timer->attr, type, *interval,
            repeats, timer->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> timer_lock(timer->mutex);
    timer->is_pulse = type != 0;
    timer->is_repeat = repeats != 0;
    timer->event_interval = *interval;

    if (timer->is_started) {
        timer_schedule_event(kernel, *timer);
    }

    return SCE_KERNEL_OK;
}

// this function is actually only called by simple_event_waitorpoll
// as the only way to wait for a timer is using the event function (a timer is an event)
SceInt32 timer_waitorpoll(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, const Address result_pattern_addr, const Address user_data_addr, SceUInt32 *timeout, bool is_wait, bool callbacks_allowed) {
    TimerPtr timer = lock_and_find(event_id, kernel.timers, kernel.mutex);
    if (!timer) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} timeout: {}"
                  " waiting_threads: {}",
            export_name, timer->uid, thread_id, timer->name, timer->attr, timeout ? *timeout : 0,
            timer->waiting_threads.size());
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);

    std::unique_lock<std::mutex> lock(timer->mutex);
    if (timer->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);

    if (result_pattern)
        *result_pattern = SCE_KERNEL_EVENT_TIMER;
    if (user_data)
        *user_data = 0;

    uint64_t current_time = kernel.core_timing.now_us();
    if (timer->next_event <= current_time && timer->next_event != std::numeric_limits<uint64_t>::max()) {
        if (!timer->is_pulse) {
            timer->event_set = true;
        }
        if (timer->is_repeat) {
            timer->next_event += ((current_time - timer->next_event) / timer->event_interval + 1) * timer->event_interval;
            arm_timer_event(kernel, *timer);
        } else {
            timer->next_event = std::numeric_limits<uint64_t>::max();
            timer->event_handle = 0;
        }
    }

    const bool timer_bit_requested = (bit_pattern & SCE_KERNEL_EVENT_TIMER) != 0;

    if (timer->event_set && timer_bit_requested) {
        if (timer->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET) {
            timer->event_set = false;
        }

        return SCE_KERNEL_OK;
    } else if (is_wait) {
        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        auto wait_state = WaitState::timer(timer->uid, bit_pattern, timeout ? *timeout : 0, timeout != nullptr, callbacks_allowed);
        wait_state.continuation.writeback.out_u32 = result_pattern_addr;
        wait_state.continuation.writeback.out_u64 = user_data_addr;
        kernel.thread_manager.begin_wait_locked(*thread, wait_state);

        const auto waiter = make_waiter_ref(*thread);
        enqueue_waiter(timer->waiting_threads, waiter, queue_uses_priority(timer->attr));
        lock.unlock();

        WaitEndReason end_reason = WaitEndReason::None;
        if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::Timer, timer->uid, timeout)) {
            kernel.thread_manager.clear_wait_locked(*thread);
            thread_lock.unlock();
            lock.lock();
            erase_waiter_if_present(timer->waiting_threads, waiter.thread_id, waiter.wait_sequence);
            return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
        }

        end_reason = thread->wait.end_reason;
        const auto wait_state_after = thread->wait;
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();
        lock.lock();
        erase_waiter_if_present(timer->waiting_threads, waiter.thread_id, waiter.wait_sequence);

        if (end_reason == WaitEndReason::Signaled) {
            write_wait_u32(mem, wait_state_after.continuation.writeback.out_u32, wait_state_after.completion.value_u32);
            write_wait_u64(mem, wait_state_after.continuation.writeback.out_u64, wait_state_after.completion.value_u64);
        }

        if (end_reason != WaitEndReason::Signaled)
            return map_wait_end_reason(end_reason, SCE_KERNEL_ERROR_WAIT_CANCEL);

        timer->event_set = timer->event_set && !timer->is_pulse && !(timer->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET);
        if (timer->event_set && !timer->waiting_threads.empty()) {
            for (auto it = timer->waiting_threads.begin(); it != timer->waiting_threads.end();) {
                const auto waiter_ref = *it;
                WaitingThreadSnapshot snapshot;
                if (!get_waiting_thread_snapshot(kernel, waiter_ref, WaitType::Timer, timer->uid, snapshot)) {
                    it = timer->waiting_threads.erase(it);
                    continue;
                }

                if ((snapshot.wait.payload.pattern & SCE_KERNEL_EVENT_TIMER) == 0) {
                    ++it;
                    continue;
                }

                WaitState::WaitCompletion completion;
                completion.value_u32 = SCE_KERNEL_EVENT_TIMER;
                completion.value_u64 = 0;
                completion.condition_met = true;
                kernel.thread_manager.complete_wait(*snapshot.thread, waiter_ref.wait_sequence, completion);
                timer->waiting_threads.erase(it);
                break;
            }
        }

        return SCE_KERNEL_OK;
    } else {
        return SCE_KERNEL_ERROR_EVENT_COND;
    }
}

// this function is actually only called by simple_event_clear
SceInt32 timer_clear(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id, SceUInt32 clear_pattern) {
    TimerPtr timer = lock_and_find(event_id, kernel.timers, kernel.mutex);
    if (!timer) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {}",
            export_name, event_id, thread_id);
    }

    std::lock_guard<std::mutex> guard(timer->mutex);
    timer->event_set = false;
    return SCE_KERNEL_OK;
}

SceInt32 timer_start(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID timer_handle) {
    TimerPtr timer = lock_and_find(timer_handle, kernel.timers, kernel.mutex);
    if (!timer)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    if (timer->is_started)
        return 1;

    const std::lock_guard<std::mutex> guard(timer->mutex);
    timer->is_started = true;
    timer->time = kernel.core_timing.now_us();

    if (timer->event_interval != 0) {
        timer_schedule_event(kernel, *timer);
    }

    return SCE_KERNEL_OK;
}

SceInt32 timer_stop(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID timer_handle) {
    const TimerPtr timer = lock_and_find(timer_handle, kernel.timers, kernel.mutex);
    if (!timer)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    const std::lock_guard<std::mutex> timer_lock(timer->mutex);
    bool was_stopped = !timer->is_started;
    timer->is_started = false;
    timer->time = kernel.core_timing.now_us();
    timer->next_event = std::numeric_limits<uint64_t>::max();
    cancel_timer_event(kernel, *timer);

    return static_cast<int>(was_stopped);
}

SceInt32 timer_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID timer_handle) {
    const TimerPtr timer = lock_and_find(timer_handle, kernel.timers, kernel.mutex);
    if (!timer)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} waiting_threads: {}",
            export_name, timer->uid, thread_id, timer->name, timer->attr, timer->waiting_threads.size());
    }

    {
        const std::lock_guard<std::mutex> timer_lock(timer->mutex);
        if (timer->deleted)
            return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

        timer->deleted = true;
        timer->is_started = false;
        timer->event_set = false;
        timer->next_event = std::numeric_limits<uint64_t>::max();
        cancel_timer_event(kernel, *timer);
        wake_waiters_for_delete(kernel, timer->waiting_threads, WaitType::Timer, timer->uid);
    }

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.timers.erase(timer->uid);
    return SCE_KERNEL_OK;
}

// *********
// * Mutex *
// *********

SceUID mutex_create(SceUID *uid_out, KernelState &kernel, MemState &mem, const char *export_name, const char *mutex_name, SceUID thread_id, SceUInt attr, int init_count, Ptr<SceKernelLwMutexWork> workarea, SyncWeight weight) {
    if ((strlen(mutex_name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }
    if (init_count < 0) {
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    }
    if (init_count > 1 && ((attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) == 0)) {
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    }

    const MutexPtr mutex = std::make_shared<Mutex>();
    const SceUID uid = kernel.get_next_uid();
    mutex->uid = uid;
    mutex->init_count = init_count;
    mutex->lock_count = init_count;
    mutex->workarea = workarea;
    strncpy(mutex->name, mutex_name, KERNELOBJECT_MAX_NAME_LENGTH);
    mutex->attr = attr;
    mutex->owner = SCE_UID_INVALID_UID;
    if (init_count > 0) {
        mutex->owner = thread_id;
    }

    if (weight == SyncWeight::Light)
        sync_lwmutex_workarea(*mutex, mem);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    auto &mutexes = get_mutexes(kernel, weight);
    mutexes.emplace(uid, mutex);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} init_count: {}",
            export_name, uid, thread_id, mutex_name, attr, init_count);
    }

    if (uid_out) {
        *uid_out = uid;
    }

    return SCE_KERNEL_OK;
}

SceUID mutex_find(KernelState &kernel, const char *export_name, const char *pName) {
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    if (LOG_SYNC_PRIMITIVES)
        LOG_DEBUG("{}: name: \"{}\"", export_name, pName);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);

    const auto it = std::find_if(kernel.mutexes.begin(), kernel.mutexes.end(), [=](const auto &mutex) {
        return strncmp(mutex.second->name, pName, KERNELOBJECT_MAX_NAME_LENGTH) == 0;
    });

    if (it != kernel.mutexes.end())
        return it->first;

    return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);
}

inline static int mutex_lock_impl(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, int lock_count, MutexPtr &mutex, SyncWeight weight, SceUInt *timeout, bool only_try, bool callbacks_allowed) {
    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} lock_count: {} timeout: {} waiting_threads: {}",
            export_name, mutex->uid, thread_id, mutex->name, mutex->attr, mutex->lock_count, timeout ? *timeout : 0,
            mutex->waiting_threads.size());
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);

    std::unique_lock<std::mutex> mutex_lock(mutex->mutex);
    if (mutex->deleted)
        return unknown_mutex_id(export_name, weight);

    bool is_recursive = (mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);
    if (lock_count < 1)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    if (lock_count != 1 && !is_recursive)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);

    // Already owned
    if (mutex->lock_count > 0) {
        // Owned by ourselves
        if (mutex->owner == thread_id) {
            if (is_recursive) {
                if (mutex->lock_count > std::numeric_limits<int>::max() - lock_count)
                    return mutex_lock_ovf_error(export_name, weight);

                mutex->lock_count += lock_count;
                if (weight == SyncWeight::Light)
                    mutex->workarea.get(mem)->lockCount += lock_count;

                return SCE_KERNEL_OK;
            }
            return mutex_recursive_error(export_name, weight);
        }
        // Owned by someone else

        // Don't sleep if only_try is set
        if (only_try)
            return mutex_failed_to_own_error(export_name, weight);

        // Sleep thread!
        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        kernel.thread_manager.begin_wait_locked(*thread, WaitState::mutex(mutex->uid, lock_count, timeout ? *timeout : 0, timeout != nullptr, callbacks_allowed));
        const auto waiter = make_waiter_ref(*thread);
        enqueue_waiter(mutex->waiting_threads, waiter, queue_uses_priority(mutex->attr));
        mutex_lock.unlock();

        WaitEndReason end_reason = WaitEndReason::None;
        if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::Mutex, mutex->uid, timeout)) {
            kernel.thread_manager.clear_wait_locked(*thread);
            thread_lock.unlock();
            mutex_lock.lock();
            erase_waiter_if_present(mutex->waiting_threads, waiter.thread_id, waiter.wait_sequence);

            return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
        }

        end_reason = thread->wait.end_reason;
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();
        mutex_lock.lock();

        if (weight == SyncWeight::Light && !mutex->deleted)
            sync_lwmutex_workarea(*mutex, mem);

        const int deleted_error = weight == SyncWeight::Light ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX : SCE_KERNEL_ERROR_WAIT_DELETE_MUTEX;
        return map_wait_end_reason(end_reason, SCE_KERNEL_ERROR_WAIT_CANCEL_MUTEX, deleted_error);
    }
    // Not owned
    // Take ownership!

    if (lock_count > std::numeric_limits<int>::max() - mutex->lock_count)
        return mutex_lock_ovf_error(export_name, weight);

    mutex->lock_count += lock_count;
    mutex->owner = thread_id;

    if (weight == SyncWeight::Light)
        sync_lwmutex_workarea(*mutex, mem);

    return SCE_KERNEL_OK;
}

int mutex_lock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID mutexid, int lock_count, unsigned int *timeout, SyncWeight weight, bool callbacks_allowed) {
    assert(mutexid >= 0);
    dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);

    MutexPtr mutex = lock_and_find(mutexid, get_mutexes(kernel, weight), kernel.mutex);
    if (!mutex)
        return unknown_mutex_id(export_name, weight);

    return mutex_lock_impl(kernel, mem, export_name, thread_id, lock_count, mutex, weight, timeout, false, callbacks_allowed);
}

int mutex_try_lock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID mutexid, int lock_count, SyncWeight weight) {
    assert(mutexid >= 0);

    MutexPtr mutex = lock_and_find(mutexid, get_mutexes(kernel, weight), kernel.mutex);
    if (!mutex)
        return unknown_mutex_id(export_name, weight);

    return mutex_lock_impl(kernel, mem, export_name, thread_id, lock_count, mutex, weight, nullptr, true, false);
}

inline static int mutex_unlock_impl(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, int unlock_count, MutexPtr &mutex, SyncWeight weight) {
    const std::lock_guard<std::mutex> mutex_lock(mutex->mutex);
    if (mutex->deleted)
        return unknown_mutex_id(export_name, weight);

    const bool is_recursive = (mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) != 0;
    if (unlock_count < 1)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    if (unlock_count != 1 && !is_recursive)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    if (mutex->owner != thread_id)
        return mutex_not_owned_error(export_name, weight);
    if (unlock_count > mutex->lock_count)
        return mutex_unlock_udf_error(export_name, weight);

    mutex->lock_count -= unlock_count;

    if (mutex->lock_count == 0) {
        mutex->owner = SCE_UID_INVALID_UID;

        if (!mutex->waiting_threads.empty()) {
            const auto waiter = mutex->waiting_threads.front();
            WaitingThreadSnapshot snapshot;
            mutex->waiting_threads.pop_front();
            if (!get_waiting_thread_snapshot(kernel, waiter, WaitType::Mutex, mutex->uid, snapshot)) {
                if (weight == SyncWeight::Light)
                    sync_lwmutex_workarea(*mutex, mem);
                return SCE_KERNEL_OK;
            }

            const auto waiting_lock_count = snapshot.wait.payload.lock_count;
            mutex->lock_count = waiting_lock_count;
            mutex->owner = snapshot.thread->id;
            WaitState::WaitCompletion completion;
            completion.condition_met = true;
            kernel.thread_manager.complete_wait(*snapshot.thread, waiter.wait_sequence, completion);
        }
    }

    if (weight == SyncWeight::Light)
        sync_lwmutex_workarea(*mutex, mem);
    return SCE_KERNEL_OK;
}

int mutex_unlock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID mutexid, int unlock_count, SyncWeight weight) {
    assert(mutexid >= 0);

    MutexPtr mutex = lock_and_find(mutexid, get_mutexes(kernel, weight), kernel.mutex);
    if (!mutex)
        return unknown_mutex_id(export_name, weight);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} lock_count: {} waiting_threads: {}",
            export_name, mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count, unlock_count,
            mutex->waiting_threads.size());
    }

    return mutex_unlock_impl(kernel, mem, export_name, thread_id, unlock_count, mutex, weight);
}

int mutex_delete(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID mutexid, SyncWeight weight) {
    assert(mutexid >= 0);

    auto &mutexes = get_mutexes(kernel, weight);
    MutexPtr mutex = lock_and_find(mutexid, mutexes, kernel.mutex);
    if (!mutex)
        return unknown_mutex_id(export_name, weight);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} lock_count: {} waiting_threads: {}",
            export_name, mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count,
            mutex->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> mutex_lock(mutex->mutex);
    if (mutex->deleted)
        return unknown_mutex_id(export_name, weight);

    mutex->deleted = true;
    if (weight == SyncWeight::Light)
        invalidate_workarea(mutex->workarea, mem);

    wake_waiters_for_delete(kernel, mutex->waiting_threads, WaitType::Mutex, mutex->uid);

    const std::lock_guard<std::mutex> kernel_guard(kernel.mutex);
    mutexes.erase(mutexid);

    return SCE_KERNEL_OK;
}

MutexPtr mutex_get(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, SyncWeight weight) {
    assert(mutexid >= 0);

    MutexPtr mutex = lock_and_find(mutexid, get_mutexes(kernel, weight), kernel.mutex);
    if (!mutex)
        return nullptr;

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} lock_count: {} waiting_threads: {}",
            export_name, mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count,
            mutex->waiting_threads.size());
    }
    return mutex;
}

// **************
// * RWLock *
// **************

SceUID rwlock_create(KernelState &kernel, MemState &mem, const char *export_name, const char *name, SceUID thread_id, SceUInt32 attr) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const RWLockPtr rwlock = std::make_shared<RWLock>();
    const SceUID uid = kernel.get_next_uid();
    rwlock->uid = uid;
    strncpy(rwlock->name, name, KERNELOBJECT_MAX_NAME_LENGTH);
    rwlock->attr = attr;
    rwlock->read_lock_count = 0;
    rwlock->write_lock_count = 0;
    rwlock->write_owner = SCE_UID_INVALID_UID;

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.rwlocks.emplace(uid, rwlock);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {}",
            export_name, uid, thread_id, name, attr);
    }

    return uid;
}

SceInt32 rwlock_lock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID lock_id, uint32_t *timeout, bool is_write, bool callbacks_allowed) {
    dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);

    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    const RWLockPtr rwlock = lock_and_find(lock_id, kernel.rwlocks, kernel.mutex);

    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} timeout: {} waiting_threads: {}",
            export_name, lock_id, thread_id, rwlock->name, rwlock->attr, timeout ? *timeout : 0,
            rwlock->waiting_threads.size());
    }

    std::unique_lock<std::mutex> rwlock_lock(rwlock->mutex);
    if (rwlock->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    const bool write_recursive = (rwlock->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) != 0;
    if (is_write) {
        if (rwlock->write_owner == thread_id) {
            if (!write_recursive)
                return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE);
            if (rwlock->write_lock_count == std::numeric_limits<int>::max())
                return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF);

            ++rwlock->write_lock_count;
            return SCE_KERNEL_OK;
        }

        if (rwlock->read_owners.contains(thread_id))
            return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE);

        if (rwlock_can_grant_write(*rwlock)) {
            rwlock->write_owner = thread_id;
            rwlock->write_lock_count = 1;
            return SCE_KERNEL_OK;
        }
    } else {
        if (rwlock->write_owner == thread_id)
            return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK);

        if (rwlock_can_grant_read(kernel, *rwlock, thread)) {
            auto &count = rwlock->read_owners[thread_id];
            if (count == std::numeric_limits<int>::max() || rwlock->read_lock_count == std::numeric_limits<int>::max())
                return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF);

            ++count;
            ++rwlock->read_lock_count;
            return SCE_KERNEL_OK;
        }
    }

    std::unique_lock<std::mutex> thread_lock(thread->mutex);
    kernel.thread_manager.begin_wait_locked(*thread, WaitState::rwlock(lock_id, is_write, timeout ? *timeout : 0, timeout != nullptr, callbacks_allowed));
    const auto waiter = make_waiter_ref(*thread);
    enqueue_waiter(rwlock->waiting_threads, waiter, queue_uses_priority(rwlock->attr));
    rwlock_lock.unlock();

    WaitEndReason end_reason = WaitEndReason::None;
    if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::RWLock, lock_id, timeout)) {
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();
        rwlock_lock.lock();
        erase_waiter_if_present(rwlock->waiting_threads, waiter.thread_id, waiter.wait_sequence);
        return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    }

    end_reason = thread->wait.end_reason;
    kernel.thread_manager.clear_wait_locked(*thread);
    thread_lock.unlock();
    return map_wait_end_reason(end_reason, SCE_KERNEL_ERROR_WAIT_CANCEL);
}

SceInt32 rwlock_unlock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID lock_id, bool is_write) {
    const RWLockPtr rwlock = lock_and_find(lock_id, kernel.rwlocks, kernel.mutex);

    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} waiting_threads: {}",
            export_name, lock_id, thread_id, rwlock->name, rwlock->attr,
            rwlock->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> rwlock_lock(rwlock->mutex);
    if (rwlock->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    bool fully_unlocked = false;
    if (is_write) {
        if (rwlock->write_owner != thread_id)
            return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED);

        if (rwlock->write_lock_count < 1)
            return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_UNLOCK_UDF);

        --rwlock->write_lock_count;
        if (rwlock->write_lock_count == 0) {
            rwlock->write_owner = SCE_UID_INVALID_UID;
            fully_unlocked = true;
        }
    } else {
        if (rwlock_is_write_locked(*rwlock))
            return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK);

        auto it = rwlock->read_owners.find(thread_id);
        if (it == rwlock->read_owners.end())
            return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK);
        if (it->second < 1 || rwlock->read_lock_count < 1)
            return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_UNLOCK_UDF);

        --it->second;
        --rwlock->read_lock_count;
        if (it->second == 0)
            rwlock->read_owners.erase(it);
        fully_unlocked = rwlock->read_lock_count == 0;
    }

    if (!fully_unlocked)
        return SCE_KERNEL_OK;

    if (rwlock->waiting_threads.empty())
        return SCE_KERNEL_OK;

    auto it = rwlock->waiting_threads.begin();
    const auto first_waiter = *it;
    WaitingThreadSnapshot first_snapshot;
    if (get_waiting_thread_snapshot(kernel, first_waiter, WaitType::RWLock, rwlock->uid, first_snapshot) && first_snapshot.wait.payload.is_write) {
        rwlock->write_owner = first_snapshot.thread->id;
        rwlock->write_lock_count = 1;
        WaitState::WaitCompletion completion;
        completion.condition_met = true;
        rwlock->waiting_threads.erase(it);
        kernel.thread_manager.complete_wait(*first_snapshot.thread, first_waiter.wait_sequence, completion);
        return SCE_KERNEL_OK;
    }

    while (it != rwlock->waiting_threads.end()) {
        const auto waiter = *it;
        WaitingThreadSnapshot snapshot;
        if (!get_waiting_thread_snapshot(kernel, waiter, WaitType::RWLock, rwlock->uid, snapshot)) {
            it = rwlock->waiting_threads.erase(it);
            continue;
        }
        if (snapshot.wait.payload.is_write)
            break;

        ++rwlock->read_owners[snapshot.thread->id];
        ++rwlock->read_lock_count;
        WaitState::WaitCompletion completion;
        completion.condition_met = true;
        it = rwlock->waiting_threads.erase(it);
        kernel.thread_manager.complete_wait(*snapshot.thread, waiter.wait_sequence, completion);
    }

    return SCE_KERNEL_OK;
}

SceInt32 rwlock_delete(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID lock_id) {
    (void)mem;
    const RWLockPtr rwlock = lock_and_find(lock_id, kernel.rwlocks, kernel.mutex);

    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} waiting_threads: {}",
            export_name, lock_id, thread_id, rwlock->name, rwlock->attr,
            rwlock->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> rwlock_lock(rwlock->mutex);
    if (rwlock->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    rwlock->deleted = true;
    wake_waiters_for_delete(kernel, rwlock->waiting_threads, WaitType::RWLock, rwlock->uid);

    const std::lock_guard<std::mutex> kernel_guard(kernel.mutex);
    kernel.rwlocks.erase(lock_id);

    return SCE_KERNEL_OK;
}

// **************
// * Semaphore *
// **************

SceUID semaphore_create(KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, int init_val, int max_val) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }
    if (init_val < 0 || max_val < 0 || init_val > max_val)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);

    const SemaphorePtr semaphore = std::make_shared<Semaphore>();
    const SceUID uid = kernel.get_next_uid();
    semaphore->uid = uid;
    semaphore->init_val = init_val;
    semaphore->val = init_val;
    semaphore->max = max_val;
    semaphore->attr = attr;
    strncpy(semaphore->name, name, KERNELOBJECT_MAX_NAME_LENGTH);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} init_val: {} max_val: {}",
            export_name, uid, thread_id, name, attr, init_val, max_val);
    }

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.semaphores.emplace(uid, semaphore);

    return uid;
}

SceUID semaphore_find(KernelState &kernel, const char *export_name, const char *pName) {
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    if (LOG_SYNC_PRIMITIVES)
        LOG_DEBUG("{}: name: \"{}\"", export_name, pName);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);

    const auto it = std::find_if(kernel.semaphores.begin(), kernel.semaphores.end(), [=](const auto &sema) {
        return strncmp(sema.second->name, pName, KERNELOBJECT_MAX_NAME_LENGTH) == 0;
    });

    if (it != kernel.semaphores.end())
        return it->first;

    return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);
}

SceInt32 semaphore_wait(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout, bool callbacks_allowed) {
    assert(semaId >= 0);
    if (needCount < 1)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);

    const SemaphorePtr semaphore = lock_and_find(semaId, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} val: {} timeout: {} waiting_threads: {}",
            export_name, semaphore->uid, thread_id, semaphore->name, semaphore->attr, semaphore->val,
            pTimeout ? *pTimeout : 0, semaphore->waiting_threads.size());
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);

    std::unique_lock<std::mutex> semaphore_lock(semaphore->mutex);
    if (semaphore->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);

    if (semaphore->val < needCount || semaphore_waits_for_queue(*semaphore, needCount, thread->priority)) {
        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        kernel.thread_manager.begin_wait_locked(*thread, WaitState::semaphore(semaphore->uid, needCount, pTimeout ? *pTimeout : 0, pTimeout != nullptr, callbacks_allowed));
        const auto waiter = make_waiter_ref(*thread);
        enqueue_waiter(semaphore->waiting_threads, waiter, queue_uses_priority(semaphore->attr));
        semaphore_lock.unlock();

        WaitEndReason end_reason = WaitEndReason::None;
        if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::Semaphore, semaphore->uid, pTimeout)) {
            kernel.thread_manager.clear_wait_locked(*thread);
            thread_lock.unlock();
            semaphore_lock.lock();
            erase_waiter_if_present(semaphore->waiting_threads, waiter.thread_id, waiter.wait_sequence);
            return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
        }

        end_reason = thread->wait.end_reason;
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();

        return map_wait_end_reason(end_reason, SCE_KERNEL_ERROR_WAIT_CANCEL);
    } else {
        semaphore->val -= needCount;
    }

    return SCE_KERNEL_OK;
}

int semaphore_signal(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid, int signal) {
    assert(semaid >= 0);
    if (signal < 0)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);

    const SemaphorePtr semaphore = lock_and_find(semaid, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} val: {} signal: {} waiting_threads: {}",
            export_name, semaphore->uid, thread_id, semaphore->name, semaphore->attr, semaphore->val, signal,
            semaphore->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> semaphore_lock(semaphore->mutex);
    if (semaphore->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);

    if (semaphore->val + signal > semaphore->max) {
        return RET_ERROR(SCE_KERNEL_ERROR_SEMA_OVF);
    }
    semaphore->val += signal;

    while (!semaphore->waiting_threads.empty()) {
        const auto waiter = semaphore->waiting_threads.front();
        WaitingThreadSnapshot snapshot;
        if (!get_waiting_thread_snapshot(kernel, waiter, WaitType::Semaphore, semaphore->uid, snapshot)) {
            semaphore->waiting_threads.pop_front();
            continue;
        }
        const auto waiting_signal_count = snapshot.wait.payload.need_count;

        if (semaphore->val < waiting_signal_count)
            break;

        WaitState::WaitCompletion completion;
        completion.condition_met = true;
        kernel.thread_manager.complete_wait(*snapshot.thread, waiter.wait_sequence, completion);

        semaphore->waiting_threads.pop_front();
        semaphore->val -= waiting_signal_count;
    }

    return SCE_KERNEL_OK;
}

int semaphore_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid) {
    assert(semaid >= 0);

    const SemaphorePtr semaphore = lock_and_find(semaid, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} val: {} waiting_threads: {}",
            export_name, semaphore->uid, thread_id, semaphore->name, semaphore->attr, semaphore->val,
            semaphore->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> semaphore_lock(semaphore->mutex);
    if (semaphore->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);

    semaphore->deleted = true;
    wake_waiters_for_delete(kernel, semaphore->waiting_threads, WaitType::Semaphore, semaphore->uid);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.semaphores.erase(semaid);

    return SCE_KERNEL_OK;
}

int semaphore_cancel(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid, SceInt32 setCount, SceUInt32 *pNumWaitThreads) {
    assert(semaid >= 0);

    const SemaphorePtr semaphore = lock_and_find(semaid, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} val: {} waiting_threads: {}",
            export_name, semaphore->uid, thread_id, semaphore->name, semaphore->attr, semaphore->val,
            semaphore->waiting_threads.size());
    }

    SceUInt32 nb_threads = 0;
    const std::lock_guard<std::mutex> semaphore_lock(semaphore->mutex);
    if (semaphore->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);

    while (!semaphore->waiting_threads.empty()) {
        const auto waiter = semaphore->waiting_threads.front();
        if (const ThreadStatePtr waiting_thread = get_waiting_thread(kernel, waiter, WaitType::Semaphore, semaphore->uid))
            kernel.thread_manager.cancel_wait(*waiting_thread, waiter.wait_sequence);

        semaphore->waiting_threads.pop_front();
        nb_threads++;
    }

    if (semaphore->val < setCount) {
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    }
    if (setCount < 0) {
        semaphore->val = semaphore->init_val;
    } else {
        semaphore->val = setCount;
    }
    if (pNumWaitThreads)
        *pNumWaitThreads = nb_threads;
    return SCE_KERNEL_OK;
}

// **********************
// * Condition Variable *
// **********************

SceUID condvar_create(SceUID *uid_out, KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, SceUID assoc_mutexid, Ptr<SceKernelLwCondWork> workarea, SyncWeight weight) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }
    MutexPtr assoc_mutex = lock_and_find(assoc_mutexid, get_mutexes(kernel, weight), kernel.mutex);
    if (!assoc_mutex)
        return unknown_mutex_id(export_name, weight);

    const SceUID uid = kernel.get_next_uid();

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} assoc_mutexid: {}",
            export_name, uid, thread_id, name, attr, assoc_mutexid);
    }

    const CondvarPtr condvar = std::make_shared<Condvar>();
    condvar->uid = uid;
    condvar->attr = attr;
    condvar->associated_mutex = std::move(assoc_mutex);
    condvar->workarea = workarea;
    strncpy(condvar->name, name, KERNELOBJECT_MAX_NAME_LENGTH);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    auto &condvars = get_condvars(kernel, weight);
    condvars.emplace(uid, condvar);

    if (uid_out)
        *uid_out = uid;
    return SCE_KERNEL_OK;
}

int condvar_wait(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID condid, SceUInt *timeout, SyncWeight weight, bool callbacks_allowed) {
    assert(condid >= 0);
    dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);

    CondvarPtr condvar = lock_and_find(condid, get_condvars(kernel, weight), kernel.mutex);
    if (!condvar)
        return unknown_cond_id(export_name, weight);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} name: \"{}\" attr: {} assoc_mutexid: {} timeout: {} waiting_threads: {}",
            export_name, condvar->uid, condvar->name, condvar->attr, condvar->associated_mutex->uid,
            timeout ? *timeout : 0, condvar->waiting_threads.size());
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);

    std::unique_lock<std::mutex> condition_variable_lock(condvar->mutex);
    if (condvar->deleted)
        return unknown_cond_id(export_name, weight);

    if (auto error = mutex_unlock_impl(kernel, mem, export_name, thread_id, 1, condvar->associated_mutex, weight))
        return error;

    std::unique_lock<std::mutex> thread_lock(thread->mutex);
    kernel.thread_manager.begin_wait_locked(*thread, WaitState::condvar(condvar->uid, timeout ? *timeout : 0, timeout != nullptr, callbacks_allowed));
    const auto waiter = make_waiter_ref(*thread);
    enqueue_waiter(condvar->waiting_threads, waiter, queue_uses_priority(condvar->attr));
    condition_variable_lock.unlock();

    WaitEndReason end_reason = WaitEndReason::None;
    if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::CondVar, condvar->uid, timeout)) {
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();
        condition_variable_lock.lock();
        erase_waiter_if_present(condvar->waiting_threads, waiter.thread_id, waiter.wait_sequence);
        return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    }

    end_reason = thread->wait.end_reason;
    kernel.thread_manager.clear_wait_locked(*thread);
    thread_lock.unlock();

    if (end_reason != WaitEndReason::Signaled) {
        const int deleted_error = weight == SyncWeight::Light ? SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND : SCE_KERNEL_ERROR_WAIT_DELETE_COND;
        const int canceled_error = SCE_KERNEL_ERROR_WAIT_CANCEL_COND;
        return map_wait_end_reason(end_reason, canceled_error, deleted_error);
    }

    const int reacquire_result = mutex_lock_impl(kernel, mem, export_name, thread_id, 1, condvar->associated_mutex, weight, timeout, false, callbacks_allowed);
    return reacquire_result;
}

int condvar_signal(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID condid, Condvar::SignalTarget signal_target, SyncWeight weight) {
    assert(condid >= 0);

    CondvarPtr condvar = lock_and_find(condid, get_condvars(kernel, weight), kernel.mutex);
    if (!condvar)
        return unknown_cond_id(export_name, weight);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} name: \"{}\" attr: {} assoc_mutexid: {} waiting_threads: {}",
            export_name, condvar->uid, condvar->name, condvar->attr, condvar->associated_mutex->uid,
            condvar->waiting_threads.size());
    }

    const auto target_type = signal_target.type;

    const std::lock_guard<std::mutex> condvar_lock(condvar->mutex);
    if (condvar->deleted)
        return unknown_cond_id(export_name, weight);

    auto &waiting_threads = condvar->waiting_threads;

    if (target_type == Condvar::SignalTarget::Type::Specific) {
        // TODO: incomplete, missing error handling for missing/stale target waiters.
        auto waiting_thread_iter = find_waiter_by_thread(waiting_threads, signal_target.thread_id);
        if (waiting_thread_iter != waiting_threads.end()) {
            const auto waiter = *waiting_thread_iter;
            if (const ThreadStatePtr waiting_thread = get_waiting_thread(kernel, waiter, WaitType::CondVar, condvar->uid)) {
                WaitState::WaitCompletion completion;
                completion.condition_met = true;
                kernel.thread_manager.complete_wait(*waiting_thread, waiter.wait_sequence, completion);
                waiting_threads.erase(waiting_thread_iter);
            } else {
                waiting_threads.erase(waiting_thread_iter);
                LOG_ERROR("{}: Target thread {} not found", export_name, signal_target.thread_id);
            }
        } else {
            LOG_ERROR("{}: Target thread {} not found", export_name, signal_target.thread_id);
        }
    } else if (target_type == Condvar::SignalTarget::Type::Any) {
        if (!waiting_threads.empty()) {
            const auto waiter = waiting_threads.front();
            if (const ThreadStatePtr waiting_thread = get_waiting_thread(kernel, waiter, WaitType::CondVar, condvar->uid)) {
                WaitState::WaitCompletion completion;
                completion.condition_met = true;
                kernel.thread_manager.complete_wait(*waiting_thread, waiter.wait_sequence, completion);
            }
            waiting_threads.pop_front();
        }
    } else {
        while (!waiting_threads.empty()) {
            const auto waiter = waiting_threads.front();
            if (const ThreadStatePtr waiting_thread = get_waiting_thread(kernel, waiter, WaitType::CondVar, condvar->uid)) {
                WaitState::WaitCompletion completion;
                completion.condition_met = true;
                kernel.thread_manager.complete_wait(*waiting_thread, waiter.wait_sequence, completion);
            }
            waiting_threads.pop_front();
        }
    }

    return SCE_KERNEL_OK;
}

int condvar_delete(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID condid, SyncWeight weight) {
    assert(condid >= 0);

    auto &condvars = get_condvars(kernel, weight);
    CondvarPtr condvar = lock_and_find(condid, condvars, kernel.mutex);
    if (!condvar)
        return unknown_cond_id(export_name, weight);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} assoc_mutexid: {} waiting_threads: {}",
            export_name, condvar->uid, thread_id, condvar->name, condvar->attr, condvar->associated_mutex->uid,
            condvar->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> condvar_lock(condvar->mutex);
    if (condvar->deleted)
        return unknown_cond_id(export_name, weight);

    condvar->deleted = true;
    if (weight == SyncWeight::Light)
        invalidate_workarea(condvar->workarea, mem);

    wake_waiters_for_delete(kernel, condvar->waiting_threads, WaitType::CondVar, condvar->uid);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    condvars.erase(condid);

    return SCE_KERNEL_OK;
}

// **************
// * Event Flag *
// **************

SceUID eventflag_clear(KernelState &kernel, const char *export_name, SceUID evfId, SceUInt32 bitPattern) {
    const EventFlagPtr event = lock_and_find(evfId, kernel.eventflags, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} bitPattern: {:#b}",
            export_name, evfId, bitPattern);
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    event->flags &= bitPattern;

    return SCE_KERNEL_OK;
}

SceUID eventflag_create(KernelState &kernel, const char *export_name, SceUID thread_id, const char *pName, SceUInt32 attr, SceUInt32 initPattern) {
    if (((attr & 0x80) == 0x80) && (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const SceUID uid = kernel.get_next_uid();

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} bitPattern: {:#b}",
            export_name, uid, thread_id, pName, attr, initPattern);
    }

    const EventFlagPtr event = std::make_shared<EventFlag>();
    event->uid = uid;
    event->init_pattern = initPattern;
    event->flags = initPattern;
    strncpy(event->name, pName, KERNELOBJECT_MAX_NAME_LENGTH);
    event->attr = attr;
    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.eventflags.emplace(uid, event);

    return uid;
}

SceUID eventflag_find(KernelState &kernel, const char *export_name, const char *pName) {
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    if (LOG_SYNC_PRIMITIVES)
        LOG_DEBUG("{}: name: \"{}\"", export_name, pName);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);

    const auto it = std::find_if(kernel.eventflags.begin(), kernel.eventflags.end(), [=](const auto &evf) {
        return strncmp(evf.second->name, pName, KERNELOBJECT_MAX_NAME_LENGTH) == 0;
    });

    if (it != kernel.eventflags.end())
        return it->first;

    return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);
}

static int eventflag_waitorpoll(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits, const Address out_bits_addr, SceUInt *timeout, bool dowait, bool callbacks_allowed) {
    assert(event_id >= 0);
    if (dowait)
        dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);

    const EventFlagPtr event = lock_and_find(event_id, kernel.eventflags, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_flags: {:#b} wait_flags: {:#b} timeout: {}"
                  " waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->flags, flags, timeout ? *timeout : 0,
            event->waiting_threads.size());
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);

    std::unique_lock<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    if (dowait && (event->attr & 0x1000) == 0 && !event->waiting_threads.empty())
        return RET_ERROR(SCE_KERNEL_ERROR_EVF_MULTI);

    bool condition;
    if (wait & SCE_EVENT_WAITOR) {
        condition = event->flags & flags;
    } else {
        condition = (event->flags & flags) == flags;
    }

    if (outBits) {
        *outBits = event->flags;
    }

    if (condition) {
        if (wait & SCE_EVENT_WAITCLEAR) {
            event->flags = 0;
        }

        if (wait & SCE_EVENT_WAITCLEAR_PAT) {
            event->flags &= ~flags;
        }

        return SCE_KERNEL_OK;
    } else if (dowait) {
        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        auto wait_state = WaitState::event_flag(event->uid, flags, wait, timeout ? *timeout : 0, timeout != nullptr, callbacks_allowed);
        wait_state.continuation.writeback.out_u32 = out_bits_addr;
        kernel.thread_manager.begin_wait_locked(*thread, wait_state);
        const auto waiter = make_waiter_ref(*thread);
        enqueue_waiter(event->waiting_threads, waiter, queue_uses_priority(event->attr));
        event_lock.unlock();

        WaitEndReason end_reason = WaitEndReason::None;
        if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::EventFlag, event->uid, timeout)) {
            kernel.thread_manager.clear_wait_locked(*thread);
            thread_lock.unlock();
            event_lock.lock();
            erase_waiter_if_present(event->waiting_threads, waiter.thread_id, waiter.wait_sequence);

            if (outBits)
                *outBits = event->flags;

            return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
        }

        end_reason = thread->wait.end_reason;
        const auto wait_state_after = thread->wait;
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();

        if (end_reason != WaitEndReason::Timeout)
            write_wait_u32(mem, wait_state_after.continuation.writeback.out_u32, wait_state_after.completion.value_u32);

        return map_wait_end_reason(end_reason, SCE_KERNEL_ERROR_WAIT_CANCEL);
    } else {
        return SCE_KERNEL_ERROR_EVF_COND;
    }
}

SceInt32 eventflag_wait(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, const Ptr<SceUInt32> pResultPat, SceUInt32 *pTimeout, bool callbacks_allowed) {
    return eventflag_waitorpoll(kernel, mem, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat.get(mem), pResultPat.address(), pTimeout, true, callbacks_allowed);
}

int eventflag_poll(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits) {
    return eventflag_waitorpoll(kernel, mem, export_name, thread_id, event_id, flags, wait, outBits, 0, 0, false, false);
}

SceInt32 eventflag_set(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID evfId, SceUInt32 bitPattern) {
    assert(evfId >= 0);

    const EventFlagPtr event = lock_and_find(evfId, kernel.eventflags, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_flags: {:#b} set_flags: {:#b}"
                  " waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->flags, bitPattern,
            event->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    event->flags |= bitPattern;

    for (auto it = event->waiting_threads.begin(); it != event->waiting_threads.end();) {
        const auto waiter = *it;
        WaitingThreadSnapshot snapshot;
        if (!get_waiting_thread_snapshot(kernel, waiter, WaitType::EventFlag, event->uid, snapshot)) {
            it = event->waiting_threads.erase(it);
            continue;
        }

        const auto waiting_flags = snapshot.wait.payload.event_flag.pattern;
        const auto waiting_mode = snapshot.wait.payload.event_flag.wait_mode;

        bool condition;
        if (waiting_mode & SCE_EVENT_WAITOR) {
            condition = event->flags & waiting_flags;
        } else {
            condition = (event->flags & waiting_flags) == waiting_flags;
        }

        if (condition) {
            WaitState::WaitCompletion completion;
            completion.value_u32 = event->flags;
            completion.condition_met = true;

            if (waiting_mode & SCE_EVENT_WAITCLEAR) {
                event->flags = 0;
            }

            if (waiting_mode & SCE_EVENT_WAITCLEAR_PAT) {
                event->flags &= ~waiting_flags;
            }
            kernel.thread_manager.complete_wait(*snapshot.thread, waiter.wait_sequence, completion);

            it = event->waiting_threads.erase(it);
        } else {
            ++it;
        }
    }

    return 0;
}

SceInt32 eventflag_cancel(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id, SceUInt32 pattern, SceUInt32 *num_wait_threads) {
    assert(event_id >= 0);

    const EventFlagPtr event = lock_and_find(event_id, kernel.eventflags, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_flags: {:#b} waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->flags, event->waiting_threads.size());
    }

    SceUInt32 nb_threads = 0;

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    while (!event->waiting_threads.empty()) {
        const auto waiter = event->waiting_threads.front();
        if (const ThreadStatePtr waiting_thread = get_waiting_thread(kernel, waiter, WaitType::EventFlag, event->uid)) {
            WaitState::WaitCompletion completion;
            completion.value_u32 = pattern;
            kernel.thread_manager.complete_wait(*waiting_thread, waiter.wait_sequence, completion, WaitEndReason::Canceled);
        }

        event->waiting_threads.pop_front();
        nb_threads++;
    }

    event->flags = pattern;

    if (num_wait_threads)
        *num_wait_threads = nb_threads;

    return SCE_KERNEL_OK;
}

int eventflag_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id) {
    assert(event_id >= 0);

    const EventFlagPtr event = lock_and_find(event_id, kernel.eventflags, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_flags: {:#b} waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->flags, event->waiting_threads.size());
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    if (event->deleted)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    event->deleted = true;
    wake_waiters_for_delete(kernel, event->waiting_threads, WaitType::EventFlag, event->uid);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.eventflags.erase(event_id);

    return SCE_KERNEL_OK;
}

// *************
// * Msg Pipe  *
// *************

SceUID msgpipe_create(KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, SceSize bufSize) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const SceUID uid = kernel.get_next_uid();

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {}",
            export_name, uid, thread_id, name, attr);
    }

    const MsgPipePtr msgpipe = std::make_shared<MsgPipe>(bufSize);

    msgpipe->attr = attr;
    msgpipe->uid = uid;
    strncpy(msgpipe->name, name, KERNELOBJECT_MAX_NAME_LENGTH);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.msgpipes.emplace(uid, msgpipe);

    return uid;
}

SceUID msgpipe_find(KernelState &kernel, const char *export_name, const char *pName) {
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    if (LOG_SYNC_PRIMITIVES)
        LOG_DEBUG("{}: name: \"{}\"", export_name, pName);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);

    const auto it = std::find_if(kernel.msgpipes.begin(), kernel.msgpipes.end(), [=](const auto &msg_pipe) {
        return strncmp(msg_pipe.second->name, pName, KERNELOBJECT_MAX_NAME_LENGTH) == 0;
    });

    if (it != kernel.msgpipes.end())
        return it->first;

    return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);
}

SceSize msgpipe_recv(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID msgPipeId, SceUInt32 waitMode, const Ptr<void> pRecvBuf, SceSize recvSize, SceUInt32 *pTimeout, const bool callbacks_allowed) {
    assert(msgPipeId >= 0);
    dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);
    void *const recv_buf = pRecvBuf.get(mem);

    const bool ASAP = !(waitMode & SCE_KERNEL_MSG_PIPE_MODE_FULL);

    const MsgPipePtr msgpipe = lock_and_find(msgPipeId, kernel.msgpipes, kernel.mutex);
    if (!msgpipe) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" pipe attr: {} wait_mode: {:#b} ({})"
                  " senders: {} receivers: {}",
            export_name, msgpipe->uid, thread_id, msgpipe->name, msgpipe->attr, waitMode, ASAP ? "ASAP" : "FULL",
            msgpipe->senders.size(), msgpipe->receivers.size());
    }

    if (recvSize > msgpipe->data_buffer.Capacity())
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_SIZE);

    const auto copyOut = [&] {
        if (waitMode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE) {
            return msgpipe->data_buffer.Peek(recv_buf, recvSize);
        } else {
            return msgpipe->data_buffer.Remove(recv_buf, recvSize);
        }
    };

    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    std::unique_lock msgpipe_lock(msgpipe->mutex);
    // check in case of delete happens while waiting (un)lock
    if (msgpipe->beingDeleted) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
    }

    const auto wakeup_senders = [&] {
        wake_first_matching_waiter(kernel, msgpipe->senders, WaitType::MsgPipeSend, msgpipe->uid, [&](const WaitingThreadSnapshot &snapshot) {
            return snapshot.wait.payload.msgpipe.request_size <= msgpipe->data_buffer.Free();
        });
    };

    std::size_t availableSize = msgpipe->data_buffer.Used();
    if ((availableSize >= recvSize) || (ASAP && availableSize >= 1)) { // Copy out and return.
        SceSize copied_size = (SceSize)copyOut();
        wakeup_senders();
        return copied_size;
    } else if (waitMode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT) {
        return 0;
    } else { // sleep until we can insert
        const SceSize requested_size = ASAP ? 1 : recvSize;
        std::unique_lock thread_lock(thread->mutex); // Lock thread - needed for condition variable
        auto wait_state = WaitState::msgpipe_receive(msgpipe->uid, requested_size, waitMode, pTimeout ? *pTimeout : 0, pTimeout != nullptr, callbacks_allowed);
        wait_state.continuation.writeback.data_addr = pRecvBuf.address();
        wait_state.continuation.requested_size = recvSize;
        kernel.thread_manager.begin_wait_locked(*thread, wait_state);
        const auto waiter = make_waiter_ref(*thread);
        enqueue_waiter(msgpipe->receivers, waiter, queue_uses_priority(msgpipe->attr));

        msgpipe_lock.unlock();

        WaitEndReason end_reason = WaitEndReason::None;
        if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::MsgPipeReceive, msgpipe->uid, pTimeout)) {
            kernel.thread_manager.clear_wait_locked(*thread);
            msgpipe_lock.lock();
            erase_waiter_if_present(msgpipe->receivers, waiter.thread_id, waiter.wait_sequence);
            msgpipe_lock.unlock();
            return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
        }

        end_reason = thread->wait.end_reason;
        const auto wait_state_after = thread->wait;
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();

        if (end_reason == WaitEndReason::Deleted) {
            notify_msgpipe_delete_waiter(*msgpipe);
            return SCE_KERNEL_ERROR_WAIT_DELETE;
        }

        if (end_reason != WaitEndReason::Signaled)
            return SCE_KERNEL_ERROR_WAIT_CANCEL;

        msgpipe_lock.lock();
        auto *recv_buf = Ptr<uint8_t>(wait_state_after.continuation.writeback.data_addr).get(mem);
        SceSize readSize = 0;
        if (wait_state_after.payload.msgpipe.wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE) {
            readSize = static_cast<SceSize>(msgpipe->data_buffer.Peek(recv_buf, wait_state_after.continuation.requested_size));
        } else {
            readSize = static_cast<SceSize>(msgpipe->data_buffer.Remove(recv_buf, wait_state_after.continuation.requested_size));
        }
        wakeup_senders();
        return readSize;
    }
}

SceSize msgpipe_send(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID msgPipeId, SceUInt32 waitMode, const Ptr<const void> pSendBuf, SceSize sendSize, SceUInt32 *pTimeout, const bool callbacks_allowed) {
    assert(msgPipeId >= 0);
    dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);
    const void *const send_buf = pSendBuf.get(mem);

    const bool ASAP = !(waitMode & SCE_KERNEL_MSG_PIPE_MODE_FULL);

    const MsgPipePtr msgpipe = lock_and_find(msgPipeId, kernel.msgpipes, kernel.mutex);
    if (!msgpipe) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" pipe attr: {} wait_mode: {:#b}"
                  " senders: {} receivers: {}",
            export_name, msgpipe->uid, thread_id, msgpipe->name, msgpipe->attr, waitMode,
            msgpipe->senders.size(), msgpipe->receivers.size());
    }

    if (sendSize > msgpipe->data_buffer.Capacity())
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_SIZE);

    const auto wakeup_receivers = [&] {
        wake_first_matching_waiter(kernel, msgpipe->receivers, WaitType::MsgPipeReceive, msgpipe->uid, [&](const WaitingThreadSnapshot &snapshot) {
            return snapshot.wait.payload.msgpipe.request_size <= msgpipe->data_buffer.Used();
        });
    };

    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    std::unique_lock<std::mutex> msgpipe_lock(msgpipe->mutex);
    // check in case of delete happens while waiting (un)lock
    if (msgpipe->beingDeleted) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
    }

    // If ASAP and there's at least 1 free byte, or FULL and there's enough space, copy and return directly.
    std::size_t freeSize = msgpipe->data_buffer.Free();
    if ((freeSize >= sendSize) || (ASAP && (freeSize >= 1))) {
        SceSize copied_size = (SceSize)msgpipe->data_buffer.Insert(send_buf, sendSize);

        wakeup_receivers();

        return copied_size;
    } else if (waitMode & SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT) {
        return 0;
    } else { // Go to sleep until there's more space
        const SceSize requested_size = ASAP ? 1 : sendSize;
        std::unique_lock thread_lock(thread->mutex); // Lock thread - needed for condition variable
        auto wait_state = WaitState::msgpipe_send(msgpipe->uid, requested_size, waitMode, pTimeout ? *pTimeout : 0, pTimeout != nullptr, callbacks_allowed);
        wait_state.continuation.writeback.data_addr = pSendBuf.address();
        wait_state.continuation.requested_size = sendSize;
        kernel.thread_manager.begin_wait_locked(*thread, wait_state);
        const auto waiter = make_waiter_ref(*thread);
        enqueue_waiter(msgpipe->senders, waiter, queue_uses_priority(msgpipe->attr));

        msgpipe_lock.unlock();

        WaitEndReason end_reason = WaitEndReason::None;
        if (!wait_for_timeout_or_signal(kernel, thread_lock, thread, WaitType::MsgPipeSend, msgpipe->uid, pTimeout)) {
            kernel.thread_manager.clear_wait_locked(*thread);
            msgpipe_lock.lock();
            erase_waiter_if_present(msgpipe->senders, waiter.thread_id, waiter.wait_sequence);
            msgpipe_lock.unlock();
            return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
        }

        end_reason = thread->wait.end_reason;
        const auto wait_state_after = thread->wait;
        kernel.thread_manager.clear_wait_locked(*thread);
        thread_lock.unlock();

        if (end_reason == WaitEndReason::Deleted) {
            notify_msgpipe_delete_waiter(*msgpipe);
            return SCE_KERNEL_ERROR_WAIT_DELETE;
        }

        if (end_reason != WaitEndReason::Signaled)
            return SCE_KERNEL_ERROR_WAIT_CANCEL;

        msgpipe_lock.lock();
        auto *send_buf = Ptr<const uint8_t>(wait_state_after.continuation.writeback.data_addr).get(mem);
        SceSize insertedSize = static_cast<SceSize>(msgpipe->data_buffer.Insert(send_buf, wait_state_after.continuation.requested_size));
        wakeup_receivers();
        return static_cast<int>(insertedSize);
    }
}

SceSize msgpipe_recv_vector(KernelState &kernel, MemState &mem, const char *export_name, const SceUID thread_id, const SceUID msgPipeId, const Ptr<const SceKernelMsgPipeVector> recvVectors, const SceUInt32 vectorCount, const SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout, const bool callbacks_allowed) {
    if (!recvVectors && vectorCount != 0)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    const auto *recv_vector_data = recvVectors.get(mem);
    std::vector<SceKernelMsgPipeVector> vectors(vectorCount);
    SceSize total_size = 0;
    for (SceUInt32 i = 0; i < vectorCount; ++i) {
        vectors[i] = recv_vector_data[i];
        total_size += vectors[i].size;
    }

    const Address scratch_addr = total_size ? alloc(mem, total_size, "msgpipe_recv_vector") : 0;
    const Ptr<uint8_t> scratch(scratch_addr);
    const auto ret = msgpipe_recv(kernel, mem, export_name, thread_id, msgPipeId, waitMode, scratch.cast<void>(), total_size, pTimeout, callbacks_allowed);
    if (static_cast<int>(ret) < 0) {
        if (scratch_addr != 0)
            free(mem, scratch_addr);
        return ret;
    }

    SceSize remaining = ret;
    SceSize copied = 0;
    auto *const scratch_data = scratch.get(mem);
    for (const auto &vector : vectors) {
        if (remaining == 0)
            break;

        const SceSize chunk = std::min(vector.size, remaining);
        if (chunk != 0)
            memcpy(Ptr<uint8_t>(vector.base).get(mem), scratch_data + copied, chunk);
        copied += chunk;
        remaining -= chunk;
    }

    if (scratch_addr != 0)
        free(mem, scratch_addr);

    if (pResult)
        *pResult = ret;
    return SCE_KERNEL_OK;
}

SceSize msgpipe_send_vector(KernelState &kernel, MemState &mem, const char *export_name, const SceUID thread_id, const SceUID msgPipeId, const Ptr<const SceKernelMsgPipeVector> sendVectors, const SceUInt32 vectorCount, const SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout, const bool callbacks_allowed) {
    if (!sendVectors && vectorCount != 0)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    const auto *send_vector_data = sendVectors.get(mem);
    std::vector<SceKernelMsgPipeVector> vectors(vectorCount);
    SceSize total_size = 0;
    for (SceUInt32 i = 0; i < vectorCount; ++i) {
        vectors[i] = send_vector_data[i];
        total_size += vectors[i].size;
    }

    const Address scratch_addr = total_size ? alloc(mem, total_size, "msgpipe_send_vector") : 0;
    const Ptr<uint8_t> scratch(scratch_addr);
    auto *const scratch_data = scratch.get(mem);
    SceSize copied = 0;
    for (const auto &vector : vectors) {
        if (vector.size == 0)
            continue;

        memcpy(scratch_data + copied, Ptr<const uint8_t>(vector.base).get(mem), vector.size);
        copied += vector.size;
    }

    const auto ret = msgpipe_send(kernel, mem, export_name, thread_id, msgPipeId, waitMode, scratch.cast<const void>(), total_size, pTimeout, callbacks_allowed);
    if (scratch_addr != 0)
        free(mem, scratch_addr);
    if (static_cast<int>(ret) < 0)
        return ret;

    if (pResult)
        *pResult = ret;
    return SCE_KERNEL_OK;
}

SceInt32 msgpipe_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID msgpipe_id) {
    assert(msgpipe_id >= 0);

    const MsgPipePtr msgpipe = lock_and_find(msgpipe_id, kernel.msgpipes, kernel.mutex);
    if (!msgpipe) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {}",
            export_name, msgpipe->uid, thread_id, msgpipe->name, msgpipe->attr);
    }

    if (!msgpipe->receivers.empty() || !msgpipe->senders.empty()) {
        std::unique_lock<std::mutex> event_lock(msgpipe->mutex);
        msgpipe->remainingThreads = 0;
        msgpipe->beingDeleted = true;
        std::atomic_thread_fence(std::memory_order_release);

        // Wake up every thread
        for (const auto &waiter : msgpipe->senders) {
            if (const ThreadStatePtr thread = get_waiting_thread(kernel, waiter, WaitType::MsgPipeSend, msgpipe->uid)) {
                msgpipe->remainingThreads.fetch_add(1, std::memory_order_relaxed);
                kernel.thread_manager.cancel_wait(*thread, waiter.wait_sequence, WaitEndReason::Deleted);
            }
        }
        for (const auto &waiter : msgpipe->receivers) {
            if (const ThreadStatePtr thread = get_waiting_thread(kernel, waiter, WaitType::MsgPipeReceive, msgpipe->uid)) {
                msgpipe->remainingThreads.fetch_add(1, std::memory_order_relaxed);
                kernel.thread_manager.cancel_wait(*thread, waiter.wait_sequence, WaitEndReason::Deleted);
            }
        }
        msgpipe->delete_cond.wait(event_lock, [&] {
            return msgpipe->remainingThreads.load(std::memory_order_acquire) == 0;
        });
        msgpipe->senders.clear();
        msgpipe->receivers.clear();
    }

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.msgpipes.erase(msgpipe->uid);

    return SCE_KERNEL_OK;
}
