// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <util/lock_and_find.h>
#include <util/log.h>

static constexpr bool LOG_SYNC_PRIMITIVES = false;

// ***********
// * Helpers *
// ***********

inline int unknown_mutex_id(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID);
    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
}

inline int unknown_cond_id(const char *export_name, SyncWeight weight) {
    if (weight == SyncWeight::Light)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID);
    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
}

inline MutexPtrs &get_mutexes(KernelState &kernel, SyncWeight weight) {
    return weight == SyncWeight::Light ? kernel.lwmutexes : kernel.mutexes;
}

inline CondvarPtrs &get_condvars(KernelState &kernel, SyncWeight weight) {
    return weight == SyncWeight::Light ? kernel.lwcondvars : kernel.condvars;
}

inline int find_mutex(MutexPtr &mutex_out, MutexPtrs **mutexes_out, KernelState &kernel, const char *export_name, SceUID mutexid, SyncWeight weight) {
    MutexPtrs &mutexes = get_mutexes(kernel, weight);
    mutex_out = lock_and_find(mutexid, mutexes, kernel.mutex);
    if (!mutex_out) {
        return unknown_mutex_id(export_name, weight);
    }

    if (mutexes_out)
        *mutexes_out = &mutexes;

    return SCE_KERNEL_OK;
}

inline int find_condvar(CondvarPtr &condvar_out, CondvarPtrs **condvars_out, KernelState &kernel, const char *export_name, SceUID condid, SyncWeight weight) {
    CondvarPtrs &condvars = get_condvars(kernel, weight);
    condvar_out = lock_and_find(condid, condvars, kernel.mutex);
    if (!condvar_out) {
        return unknown_cond_id(export_name, weight);
    }

    if (condvars_out)
        *condvars_out = &condvars;

    return SCE_KERNEL_OK;
}

// TODO: Write remaining time to timeout ptr when it's successfully signaled
// Assumes primitive_lock is locked and thread_lock is unlocked
inline int handle_timeout(const ThreadStatePtr &thread, std::unique_lock<std::mutex> &thread_lock,
    std::unique_lock<std::mutex> &primitive_lock, WaitingThreadQueuePtr &queue,
    const WaitingThreadData &data, const ThreadDataQueueInterator<WaitingThreadData> &data_it,
    const char *export_name, SceUInt *const timeout) {
    if (timeout) {
        bool status = false;

        if (*timeout > 0) {
            status = thread->status_cond.wait_for(primitive_lock, std::chrono::microseconds{ *timeout }, [&] { return thread->status == ThreadStatus::run; });
        }

        if (!status) {
            *timeout = 0; // Time run out, so remaining time is 0

            thread_lock.lock();
            thread->update_status(ThreadStatus::run, ThreadStatus::wait);
            thread_lock.unlock();

            queue->erase(data_it);

            return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
        }
    } else {
        thread->status_cond.wait(primitive_lock, [&] { return thread->status == ThreadStatus::run; });
    }

    return SCE_KERNEL_OK;
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
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, event->name);
    event->attr = attr;
    if (event->attr & SCE_KERNEL_ATTR_TH_PRIO) {
        event->waiting_threads = std::make_unique<PriorityThreadDataQueue<WaitingThreadData>>();
    } else {
        event->waiting_threads = std::make_unique<FIFOThreadDataQueue<WaitingThreadData>>();
    }

    event->last_user_data = 0;
    event->auto_reset = (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET);
    event->cb_wakeup_only = (event->attr & SCE_KERNEL_ATTR_NOTIFY_CB_WAKEUP_ONLY);

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.simple_events.emplace(uid, event);

    return uid;
}

SceInt32 simple_event_waitorpoll(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id, SceUInt32 wait_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool is_wait) {
    const SimpleEventPtr event = lock_and_find(event_id, kernel.simple_events, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_pattern: {:#b} wait_pattern: {:#b} timeout: {}"
                  " waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->pattern, wait_pattern, timeout ? *timeout : 0,
            event->waiting_threads->size());
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);

    std::unique_lock<std::mutex> event_lock(event->mutex);

    if (result_pattern)
        *result_pattern = event->pattern;

    if (event->pattern & wait_pattern) {
        if (event->auto_reset) {
            SceUInt32 common = event->pattern & wait_pattern;
            SceUInt32 first_common_bit = common & (-common);
            event->pattern &= ~first_common_bit;
        }

        if (user_data)
            *user_data = event->last_user_data;

        return SCE_KERNEL_OK;
    } else if (is_wait) {
        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        thread->update_status(ThreadStatus::wait, ThreadStatus::run);

        WaitingThreadData data;
        data.thread = thread;
        data.result_pattern = result_pattern;
        data.user_data = user_data;
        data.pattern = wait_pattern;
        data.priority = thread->priority;

        const auto data_it = event->waiting_threads->push(data);
        thread_lock.unlock();

        const int err = handle_timeout(thread, thread_lock, event_lock, event->waiting_threads, data, data_it, export_name, timeout);
        if (err < 0) {
            // set it only if a timeout occurs
            // otherwise set in simple_event_setorpulse
            if (user_data)
                *user_data = event->last_user_data;
            if (result_pattern)
                *result_pattern = event->pattern;
        }
        return err;
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
            event->waiting_threads->size());
    }

    const SceUInt32 old_pattern = event->pattern;
    const SceUInt64 old_user_data = event->last_user_data;
    const SceUInt32 new_pattern = event->pattern | pattern;

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    event->pattern = new_pattern;
    event->last_user_data = user_data;

    for (auto it = event->waiting_threads->begin(); it != event->waiting_threads->end();) {
        const auto waiting_thread_data = *it;
        const auto waiting_thread = waiting_thread_data.thread;
        const auto waiting_pattern = waiting_thread_data.pattern;

        if (event->pattern & waiting_pattern) {
            if (waiting_thread_data.result_pattern)
                *waiting_thread_data.result_pattern = new_pattern;

            if (waiting_thread_data.user_data)
                *waiting_thread_data.user_data = event->last_user_data;

            if (event->auto_reset) {
                SceUInt32 common = event->pattern & waiting_pattern;
                SceUInt32 common_first_bit = common & (-common);
                event->pattern &= ~common_first_bit;
            }

            const std::lock_guard<std::mutex> waiting_thread_lock(waiting_thread->mutex);

            waiting_thread->update_status(ThreadStatus::run, ThreadStatus::wait);

            event->waiting_threads->erase(it++);
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
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} clear_pattern: {:#b}",
            export_name, event_id, thread_id, clear_pattern);
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);

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
            export_name, event->uid, thread_id, event->name, event->attr, event->pattern, event->waiting_threads->size());
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);

    if (event->waiting_threads->empty()) {
        const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
        kernel.eventflags.erase(event_id);
    } else {
        // TODO:
        LOG_WARN("Can't delete sync object, it has waiting threads.");
    }

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
    if (init_count > 1 && (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE)) {
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    }

    const MutexPtr mutex = std::make_shared<Mutex>();
    const SceUID uid = kernel.get_next_uid();
    mutex->uid = uid;
    mutex->init_count = init_count;
    mutex->lock_count = init_count;
    mutex->workarea = workarea;
    std::copy(mutex_name, mutex_name + KERNELOBJECT_MAX_NAME_LENGTH, mutex->name);
    mutex->attr = attr;
    mutex->owner = nullptr;
    if (init_count > 0) {
        const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);
        mutex->owner = thread;
    }
    if (mutex->attr & SCE_KERNEL_ATTR_TH_PRIO) {
        mutex->waiting_threads = std::make_unique<PriorityThreadDataQueue<WaitingThreadData>>();
    } else {
        mutex->waiting_threads = std::make_unique<FIFOThreadDataQueue<WaitingThreadData>>();
    }

    if (weight == SyncWeight::Light) {
        SceKernelLwMutexWork *workarea_mem = workarea.get(mem);
        workarea_mem->lockCount = init_count;
        if (workarea_mem->lockCount)
            workarea_mem->owner = thread_id;
        workarea_mem->attr = attr;
    }

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

inline int mutex_lock_impl(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, int lock_count, MutexPtr &mutex, SyncWeight weight, SceUInt *timeout, bool only_try) {
    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} lock_count: {} timeout: {} waiting_threads: {}",
            export_name, mutex->uid, thread_id, mutex->name, mutex->attr, mutex->lock_count, timeout ? *timeout : 0,
            mutex->waiting_threads->size());
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);

    std::unique_lock<std::mutex> mutex_lock(mutex->mutex);

    bool is_recursive = (mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);

    // Already owned
    if (mutex->lock_count > 0) {
        // Owned by ourselves
        if (mutex->owner == thread) {
            if (is_recursive) {
                mutex->lock_count += lock_count;
                if (weight == SyncWeight::Light)
                    mutex->workarea.get(mem)->lockCount += lock_count;

                return SCE_KERNEL_OK;
            }
            if (weight == SyncWeight::Light)
                return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE);

            return RET_ERROR(SCE_KERNEL_ERROR_MUTEX_RECURSIVE);
        }
        // Owned by someone else

        // Don't sleep if only_try is set
        if (only_try) {
            if (weight == SyncWeight::Light)
                return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN);

            return RET_ERROR(SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN);
        }

        // Sleep thread!
        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        thread->update_status(ThreadStatus::wait, ThreadStatus::run);

        WaitingThreadData data;
        data.thread = thread;
        data.lock_count = lock_count;
        data.priority = thread->priority;

        const auto data_it = mutex->waiting_threads->push(data);
        thread_lock.unlock();

        int res = handle_timeout(thread, thread_lock, mutex_lock, mutex->waiting_threads, data, data_it, export_name, timeout);

        if (weight == SyncWeight::Light) {
            mutex->workarea.get(mem)->lockCount = mutex->lock_count;
            if (mutex->owner == thread) {
                mutex->workarea.get(mem)->owner = thread_id;
            }
        }

        return res;
    }
    // Not owned
    // Take ownership!

    mutex->lock_count += lock_count;
    mutex->owner = thread;

    if (weight == SyncWeight::Light) {
        mutex->workarea.get(mem)->lockCount = mutex->lock_count;
        if (mutex->owner == thread) {
            mutex->workarea.get(mem)->owner = thread_id;
        }
    }

    return SCE_KERNEL_OK;
}

int mutex_lock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID mutexid, int lock_count, unsigned int *timeout, SyncWeight weight) {
    assert(mutexid >= 0);

    MutexPtr mutex;
    if (auto error = find_mutex(mutex, nullptr, kernel, export_name, mutexid, weight))
        return error;

    return mutex_lock_impl(kernel, mem, export_name, thread_id, lock_count, mutex, weight, timeout, false);
}

int mutex_try_lock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID mutexid, int lock_count, SyncWeight weight) {
    assert(mutexid >= 0);

    MutexPtr mutex;
    if (auto error = find_mutex(mutex, nullptr, kernel, export_name, mutexid, weight))
        return error;

    return mutex_lock_impl(kernel, mem, export_name, thread_id, lock_count, mutex, weight, nullptr, true);
}

inline int mutex_unlock_impl(KernelState &kernel, const char *export_name, SceUID thread_id, int unlock_count, MutexPtr &mutex) {
    const ThreadStatePtr current_thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);

    const std::lock_guard<std::mutex> mutex_lock(mutex->mutex);

    if (current_thread == mutex->owner) {
        if (unlock_count > mutex->lock_count) {
            return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF);
        }

        mutex->lock_count -= unlock_count;

        if (mutex->lock_count == 0) {
            mutex->owner = nullptr;

            if (!mutex->waiting_threads->empty()) {
                const auto waiting_thread_data = *mutex->waiting_threads->begin();
                const auto waiting_thread = waiting_thread_data.thread;
                const auto waiting_lock_count = waiting_thread_data.lock_count;

                const std::lock_guard<std::mutex> waiting_thread_lock(waiting_thread->mutex);
                waiting_thread->update_status(ThreadStatus::run, ThreadStatus::wait);

                mutex->waiting_threads->pop();
                mutex->lock_count += waiting_lock_count;
                mutex->owner = waiting_thread;
            }
        }
    }

    return SCE_KERNEL_OK;
}

int mutex_unlock(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, int unlock_count, SyncWeight weight) {
    assert(mutexid >= 0);

    MutexPtr mutex;
    if (auto error = find_mutex(mutex, nullptr, kernel, export_name, mutexid, weight))
        return error;

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} lock_count: {} waiting_threads: {}",
            export_name, mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count, unlock_count,
            mutex->waiting_threads->size());
    }

    return mutex_unlock_impl(kernel, export_name, thread_id, unlock_count, mutex);
}

int mutex_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, SyncWeight weight) {
    assert(mutexid >= 0);

    MutexPtr mutex;
    MutexPtrs *mutexes;
    if (auto error = find_mutex(mutex, &mutexes, kernel, export_name, mutexid, weight))
        return error;

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} lock_count: {} waiting_threads: {}",
            export_name, mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count,
            mutex->waiting_threads->size());
    }

    if (mutex->waiting_threads->empty()) {
        const std::lock_guard<std::mutex> mutex_lock(mutex->mutex);
        mutexes->erase(mutexid);
    } else {
        // TODO:
        LOG_WARN("Can't delete sync object, it has waiting threads.");
    }

    return SCE_KERNEL_OK;
}

MutexPtr mutex_get(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutexid, SyncWeight weight) {
    assert(mutexid >= 0);

    MutexPtr mutex;
    MutexPtrs *mutexes;
    if (auto error = find_mutex(mutex, &mutexes, kernel, export_name, mutexid, weight))
        return nullptr;

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} lock_count: {} waiting_threads: {}",
            export_name, mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count,
            mutex->waiting_threads->size());
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
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, rwlock->name);
    rwlock->attr = attr;

    if (rwlock->attr & SCE_KERNEL_ATTR_TH_PRIO) {
        rwlock->waiting_threads = std::make_unique<PriorityThreadDataQueue<WaitingThreadData>>();
    } else {
        rwlock->waiting_threads = std::make_unique<FIFOThreadDataQueue<WaitingThreadData>>();
    }

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.rwlocks.emplace(uid, rwlock);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {}",
            export_name, uid, thread_id, name, attr);
    }

    return uid;
}

SceInt32 rwlock_lock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID lock_id, uint32_t *timeout, bool is_write) {
    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    const RWLockPtr rwlock = lock_and_find(lock_id, kernel.rwlocks, kernel.mutex);

    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} timeout: {} waiting_threads: {}",
            export_name, lock_id, thread_id, rwlock->name, rwlock->attr, timeout ? *timeout : 0,
            rwlock->waiting_threads->size());
    }

    std::unique_lock<std::mutex> rwlock_lock(rwlock->mutex);

    // if it is a read lock, it is always recursive
    bool is_recursive = !is_write || (rwlock->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);

    // cases where we don't need to wait :
    if (rwlock->state == RWLockState::Unlocked // the lock is unlocked
        || (!is_write && rwlock->state == RWLockState::ReadLocked) // we want a read lock when the lock is readlocked
        || (is_recursive && rwlock->owners.count(thread) > 0)) { // the thread asking has already locked this lock

        auto it = rwlock->owners.find(thread);
        if (it != rwlock->owners.end()) {
            // increase the count
            it->second++;
        } else {
            rwlock->owners.emplace(thread, 1);
        }

        rwlock->state = is_write ? RWLockState::WriteLocked : RWLockState::ReadLocked;

        return SCE_KERNEL_OK;
    } else if (!is_recursive && rwlock->owners.count(thread) > 0) {
        return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE);
    } else {
        // we need to wait

        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        thread->update_status(ThreadStatus::wait, ThreadStatus::run);

        WaitingThreadData data;
        data.thread = thread;
        data.is_write = is_write;
        data.priority = thread->priority;

        const auto data_it = rwlock->waiting_threads->push(data);
        thread_lock.unlock();

        return handle_timeout(thread, thread_lock, rwlock_lock, rwlock->waiting_threads, data, data_it, export_name, timeout);
    }
}

SceInt32 rwlock_unlock(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID lock_id, bool is_write) {
    const ThreadStatePtr current_thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);
    const RWLockPtr rwlock = lock_and_find(lock_id, kernel.rwlocks, kernel.mutex);

    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} waiting_threads: {}",
            export_name, lock_id, thread_id, rwlock->name, rwlock->attr,
            rwlock->waiting_threads->size());
    }

    const std::lock_guard<std::mutex> rwlock_lock(rwlock->mutex);

    auto it = rwlock->owners.find(current_thread);
    if (it == rwlock->owners.end()) {
        return RET_ERROR(SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK);
    }

    // decrease the lock count
    it->second--;
    if (it->second == 0)
        rwlock->owners.erase(current_thread);

    // if it is still locked
    if (!rwlock->owners.empty())
        return SCE_KERNEL_OK;

    rwlock->state = RWLockState::Unlocked;

    if (!rwlock->waiting_threads->empty()) {
        for (auto it = rwlock->waiting_threads->begin(); it != rwlock->waiting_threads->end();) {
            const auto &waiting_thread_data = *it;
            const auto waiting_thread = waiting_thread_data.thread;
            const auto waiting_is_write = waiting_thread_data.is_write;

            if (rwlock->state == RWLockState::ReadLocked && waiting_is_write) {
                // only awaken read threads
                it++;
                continue;
            }

            auto old_it = it;
            it++;
            rwlock->waiting_threads->erase(old_it);

            const std::lock_guard<std::mutex> waiting_thread_lock(waiting_thread->mutex);
            waiting_thread->update_status(ThreadStatus::run, ThreadStatus::wait);
            rwlock->owners.emplace(waiting_thread, 1);

            if (waiting_is_write) {
                rwlock->state = RWLockState::WriteLocked;
                break;
            }
            rwlock->state = RWLockState::ReadLocked;
        }
    }

    return SCE_KERNEL_OK;
}

SceInt32 rwlock_delete(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID lock_id) {
    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    const RWLockPtr rwlock = lock_and_find(lock_id, kernel.rwlocks, kernel.mutex);

    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} waiting_threads: {}",
            export_name, lock_id, thread_id, rwlock->name, rwlock->attr,
            rwlock->waiting_threads->size());
    }

    const std::lock_guard<std::mutex> mutex_lock(rwlock->mutex);

    if (rwlock->waiting_threads->empty()) {
        const std::lock_guard<std::mutex> kernel_guard(kernel.mutex);
        kernel.rwlocks.erase(lock_id);
    } else {
        // TODO:
        LOG_WARN("Can't delete sync object, it has waiting threads.");
    }

    return SCE_KERNEL_OK;
}

// **************
// * Semaphore *
// **************

SceUID semaphore_create(KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, int init_val, int max_val) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const SemaphorePtr semaphore = std::make_shared<Semaphore>();
    const SceUID uid = kernel.get_next_uid();
    semaphore->uid = uid;
    semaphore->val = init_val;
    semaphore->max = max_val;
    semaphore->attr = attr;
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, semaphore->name);

    if (semaphore->attr & SCE_KERNEL_ATTR_TH_PRIO) {
        semaphore->waiting_threads = std::make_unique<PriorityThreadDataQueue<WaitingThreadData>>();
    } else {
        semaphore->waiting_threads = std::make_unique<FIFOThreadDataQueue<WaitingThreadData>>();
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} init_val: {} max_val: {}",
            export_name, uid, thread_id, name, attr, init_val, max_val);
    }

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.semaphores.emplace(uid, semaphore);

    return uid;
}

SceInt32 semaphore_wait(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout) {
    assert(semaId >= 0);

    // TODO Don't lock twice.
    const SemaphorePtr semaphore = lock_and_find(semaId, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} val: {} timeout: {} waiting_threads: {}",
            export_name, semaphore->uid, thread_id, semaphore->name, semaphore->attr, semaphore->val,
            pTimeout ? *pTimeout : 0, semaphore->waiting_threads->size());
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);

    std::unique_lock<std::mutex> semaphore_lock(semaphore->mutex);

    if (semaphore->val < needCount) {
        std::unique_lock<std::mutex> thread_lock(thread->mutex);
        thread->update_status(ThreadStatus::wait, ThreadStatus::run);

        WaitingThreadData data;
        data.thread = thread;
        data.priority = thread->priority;
        data.signal = needCount;

        const auto data_it = semaphore->waiting_threads->push(data);
        thread_lock.unlock();

        return handle_timeout(thread, thread_lock, semaphore_lock, semaphore->waiting_threads, data, data_it, export_name, pTimeout);
    } else {
        semaphore->val -= needCount;
    }

    return SCE_KERNEL_OK;
}

int semaphore_signal(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid, int signal) {
    assert(semaid >= 0);

    // TODO Don't lock twice.
    const SemaphorePtr semaphore = lock_and_find(semaid, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} val: {} signal: {} waiting_threads: {}",
            export_name, semaphore->uid, thread_id, semaphore->name, semaphore->attr, semaphore->val, signal,
            semaphore->waiting_threads->size());
    }

    const std::lock_guard<std::mutex> semaphore_lock(semaphore->mutex);

    if (semaphore->val + signal > semaphore->max) {
        return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF);
    }
    semaphore->val += signal;

    while (!semaphore->waiting_threads->empty()) {
        const auto waiting_thread_data = *semaphore->waiting_threads->begin();
        const auto waiting_thread = waiting_thread_data.thread;
        const auto waiting_signal_count = waiting_thread_data.signal;

        if (semaphore->val < waiting_signal_count)
            break;

        const std::unique_lock<std::mutex> waiting_thread_lock(waiting_thread->mutex, std::try_to_lock);
        if (!waiting_thread_lock)
            continue;

        waiting_thread->update_status(ThreadStatus::run, ThreadStatus::wait);

        semaphore->waiting_threads->pop();
        semaphore->val -= waiting_signal_count;
    }

    return SCE_KERNEL_OK;
}

int semaphore_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid) {
    assert(semaid >= 0);

    // TODO: Don't lock twice
    const SemaphorePtr semaphore = lock_and_find(semaid, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} val: {} waiting_threads: {}",
            export_name, semaphore->uid, thread_id, semaphore->name, semaphore->attr, semaphore->val,
            semaphore->waiting_threads->size());
    }

    if (semaphore->waiting_threads->empty()) {
        const std::lock_guard<std::mutex> semaphore_lock(semaphore->mutex);
        kernel.semaphores.erase(semaid);
    } else {
        // TODO:
        LOG_WARN("Can't delete sync object, it has waiting threads.");
    }

    return SCE_KERNEL_OK;
}

// **********************
// * Condition Variable *
// **********************

SceUID condvar_create(SceUID *uid_out, KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUInt attr, SceUID assoc_mutexid, SyncWeight weight) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }
    MutexPtr assoc_mutex;
    if (auto error = find_mutex(assoc_mutex, nullptr, kernel, export_name, assoc_mutexid, weight))
        return error;

    const SceUID uid = kernel.get_next_uid();

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} assoc_mutexid: {}",
            export_name, uid, thread_id, name, attr, assoc_mutexid);
    }

    const CondvarPtr condvar = std::make_shared<Condvar>();
    condvar->attr = attr;
    condvar->associated_mutex = std::move(assoc_mutex);
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, condvar->name);

    if (condvar->attr & SCE_KERNEL_ATTR_TH_PRIO) {
        condvar->waiting_threads = std::make_unique<PriorityThreadDataQueue<WaitingThreadData>>();
    } else {
        condvar->waiting_threads = std::make_unique<FIFOThreadDataQueue<WaitingThreadData>>();
    }

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    auto &condvars = get_condvars(kernel, weight);
    condvars.emplace(uid, condvar);

    if (uid_out)
        *uid_out = uid;
    return SCE_KERNEL_OK;
}

int condvar_wait(KernelState &kernel, MemState &mem, const char *export_name, SceUID thread_id, SceUID condid, SceUInt *timeout, SyncWeight weight) {
    assert(condid >= 0);

    CondvarPtr condvar;
    CondvarPtrs *condvars;
    if (auto error = find_condvar(condvar, &condvars, kernel, export_name, condid, weight))
        return error;

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} name: \"{}\" attr: {} assoc_mutexid: {} timeout: {} waiting_threads: {}",
            export_name, condvar->uid, condvar->name, condvar->attr, condvar->associated_mutex->uid,
            timeout ? *timeout : 0, condvar->waiting_threads->size());
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);

    std::unique_lock<std::mutex> condition_variable_lock(condvar->mutex);

    if (auto error = mutex_unlock_impl(kernel, export_name, thread_id, 1, condvar->associated_mutex))
        return error;

    std::unique_lock<std::mutex> thread_lock(thread->mutex);
    thread->update_status(ThreadStatus::wait, ThreadStatus::run);

    WaitingThreadData data;
    data.thread = thread;
    data.priority = thread->priority;

    const auto data_it = condvar->waiting_threads->push(data);
    thread_lock.unlock();

    if (auto error = handle_timeout(thread, thread_lock, condition_variable_lock, condvar->waiting_threads, data, data_it, export_name, timeout))
        return error;

    condition_variable_lock.unlock();
    return mutex_lock_impl(kernel, mem, export_name, thread_id, 1, condvar->associated_mutex, weight, timeout, false);
}

int condvar_signal(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID condid, Condvar::SignalTarget signal_target, SyncWeight weight) {
    assert(condid >= 0);

    CondvarPtr condvar;
    CondvarPtrs *condvars;
    if (auto error = find_condvar(condvar, &condvars, kernel, export_name, condid, weight))
        return error;

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} name: \"{}\" attr: {} assoc_mutexid: {} waiting_threads: {}",
            export_name, condvar->uid, condvar->name, condvar->attr, condvar->associated_mutex->uid,
            condvar->waiting_threads->size());
    }

    const auto target_type = signal_target.type;

    const std::lock_guard<std::mutex> condvar_lock(condvar->mutex);
    auto &waiting_threads = condvar->waiting_threads;

    if (target_type == Condvar::SignalTarget::Type::Specific) {
        ThreadStatePtr waiting_thread = lock_and_find(signal_target.thread_id, kernel.threads, kernel.mutex);
        // Search for specified waiting thread
        auto waiting_thread_iter = waiting_threads->find(waiting_thread);
        if (waiting_thread_iter != waiting_threads->end()) {
            const std::lock_guard<std::mutex> waiting_thread_lock(waiting_thread->mutex);

            waiting_thread->update_status(ThreadStatus::run, ThreadStatus::wait);
            waiting_threads->erase(waiting_thread_iter);
        } else {
            LOG_ERROR("{}: Target thread {} not found", export_name, waiting_thread->name);
        }
    } else {
        while (!waiting_threads->empty()) {
            const auto waiting_thread_data = *waiting_threads->begin();
            auto waiting_thread = waiting_thread_data.thread;
            const std::unique_lock<std::mutex> waiting_thread_lock(waiting_thread->mutex, std::try_to_lock);
            if (!waiting_thread_lock)
                continue;

            waiting_thread->update_status(ThreadStatus::run, ThreadStatus::wait);
            waiting_threads->pop();
        }
    }

    return SCE_KERNEL_OK;
}

int condvar_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID condid, SyncWeight weight) {
    assert(condid >= 0);

    CondvarPtr condvar;
    CondvarPtrs *condvars;
    if (auto error = find_condvar(condvar, &condvars, kernel, export_name, condid, weight))
        return error;

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} assoc_mutexid: {} waiting_threads: {}",
            export_name, condvar->uid, thread_id, condvar->name, condvar->attr, condvar->associated_mutex->uid,
            condvar->waiting_threads->size());
    }

    if (condvar->waiting_threads->empty()) {
        const std::lock_guard<std::mutex> condvar_lock(condvar->mutex);
        condvars->erase(condid);
    } else {
        // TODO:
        LOG_WARN("Can't delete sync object, it has waiting threads.");
    }

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

    event->flags &= bitPattern;

    return SCE_KERNEL_OK;
}

SceUID eventflag_create(KernelState &kernel, const char *export_name, SceUID thread_id, const char *pName, SceUInt32 attr, SceUInt32 initPattern) {
    if ((strlen(pName) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const SceUID uid = kernel.get_next_uid();

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} bitPattern: {:#b}",
            export_name, uid, thread_id, pName, attr, initPattern);
    }

    const EventFlagPtr event = std::make_shared<EventFlag>();
    event->uid = uid;
    event->flags = initPattern;
    std::copy(pName, pName + KERNELOBJECT_MAX_NAME_LENGTH, event->name);
    event->attr = attr;
    if (event->attr & SCE_KERNEL_ATTR_TH_PRIO) {
        event->waiting_threads = std::make_unique<PriorityThreadDataQueue<WaitingThreadData>>();
    } else {
        event->waiting_threads = std::make_unique<FIFOThreadDataQueue<WaitingThreadData>>();
    }

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.eventflags.emplace(uid, event);

    return uid;
}

static int eventflag_waitorpoll(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits, SceUInt *timeout, bool dowait) {
    assert(event_id >= 0);

    // TODO Don't lock twice.
    const EventFlagPtr event = lock_and_find(event_id, kernel.eventflags, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_flags: {:#b} wait_flags: {:#b} timeout: {}"
                  " waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->flags, flags, timeout ? *timeout : 0,
            event->waiting_threads->size());
    }

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);

    std::unique_lock<std::mutex> event_lock(event->mutex);

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
        thread->update_status(ThreadStatus::wait, ThreadStatus::run);

        WaitingThreadData data;
        data.thread = thread;
        data.wait = wait;
        data.flags = flags;
        data.outBits = outBits;
        data.priority = thread->priority;

        const auto data_it = event->waiting_threads->push(data);
        thread_lock.unlock();

        const int err = handle_timeout(thread, thread_lock, event_lock, event->waiting_threads, data, data_it, export_name, timeout);
        if (err < 0 && outBits) {
            // set it only if a timeout occurs
            // otherwise set in eventflag_set
            *outBits = event->flags;
        }
        return err;
    } else {
        return SCE_KERNEL_ERROR_EVF_COND;
    }
}

SceInt32 eventflag_wait(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    return eventflag_waitorpoll(kernel, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat, pTimeout, true);
}

int eventflag_poll(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits) {
    return eventflag_waitorpoll(kernel, export_name, thread_id, event_id, flags, wait, outBits, 0, false);
}

SceInt32 eventflag_set(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID evfId, SceUInt32 bitPattern) {
    assert(evfId >= 0);

    // TODO Don't lock twice.
    const EventFlagPtr event = lock_and_find(evfId, kernel.eventflags, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_flags: {:#b} set_flags: {:#b}"
                  " waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->flags, bitPattern,
            event->waiting_threads->size());
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);
    event->flags |= bitPattern;

    for (auto it = event->waiting_threads->begin(); it != event->waiting_threads->end();) {
        const auto waiting_thread_data = *it;
        const auto waiting_thread = waiting_thread_data.thread;
        const auto waiting_flags = waiting_thread_data.flags;

        bool condition;
        if (waiting_thread_data.wait & SCE_EVENT_WAITOR) {
            condition = event->flags & waiting_flags;
        } else {
            condition = (event->flags & waiting_flags) == waiting_flags;
        }

        if (condition) {
            if (waiting_thread_data.outBits) {
                *waiting_thread_data.outBits = event->flags;
            }

            if (waiting_thread_data.wait & SCE_EVENT_WAITCLEAR) {
                event->flags = 0;
            }

            if (waiting_thread_data.wait & SCE_EVENT_WAITCLEAR_PAT) {
                event->flags &= ~waiting_flags;
            }

            const std::lock_guard<std::mutex> waiting_thread_lock(waiting_thread->mutex);

            waiting_thread->update_status(ThreadStatus::run, ThreadStatus::wait);

            event->waiting_threads->erase(it++);
        } else {
            ++it;
        }
    }

    return 0;
}

int eventflag_delete(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID event_id) {
    assert(event_id >= 0);

    const EventFlagPtr event = lock_and_find(event_id, kernel.eventflags, kernel.mutex);
    if (!event) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {} existing_flags: {:#b} waiting_threads: {}",
            export_name, event->uid, thread_id, event->name, event->attr, event->flags, event->waiting_threads->size());
    }

    const std::lock_guard<std::mutex> event_lock(event->mutex);

    if (event->waiting_threads->empty()) {
        const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
        kernel.eventflags.erase(event_id);
    } else {
        // TODO:
        LOG_WARN("Can't delete sync object, it has waiting threads.");
    }

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
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, msgpipe->name);

    if (msgpipe->attr & SCE_KERNEL_ATTR_TH_PRIO) {
        msgpipe->receivers = std::make_unique<PriorityThreadDataQueue<WaitingThreadData>>();
    } else {
        msgpipe->receivers = std::make_unique<FIFOThreadDataQueue<WaitingThreadData>>();
    }

    // TODO do senders respect priority?
    msgpipe->senders = std::make_unique<FIFOThreadDataQueue<WaitingThreadData>>();

    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
    kernel.msgpipes.emplace(uid, msgpipe);

    return uid;
}

SceUID msgpipe_find(KernelState &kernel, const char *export_name, const char *name) {
    const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);

    // TODO use another map
    const auto it = std::find_if(kernel.msgpipes.begin(), kernel.msgpipes.end(), [=](auto it) {
        return strcmp(it.second->name, name) == 0;
    });

    if (it != kernel.msgpipes.end()) {
        return it->first;
    }

    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
}

int msgpipe_recv(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID msgpipe_id, SceUInt32 wait_mode, char *recv_buf, SceSize msg_size, SceUInt32 *timeout) {
    assert(msgpipe_id >= 0);

    const bool ASAP = !(wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);

    const MsgPipePtr msgpipe = lock_and_find(msgpipe_id, kernel.msgpipes, kernel.mutex);
    if (!msgpipe) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" pipe attr: {} wait_mode: {:#b} ({})"
                  " senders: {} receivers: {}",
            export_name, msgpipe->uid, thread_id, msgpipe->name, msgpipe->attr, wait_mode, ASAP ? "ASAP" : "FULL",
            msgpipe->senders->size(), msgpipe->receivers->size());
    }

    if (msg_size > msgpipe->data_buffer.Capacity())
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_SIZE);

    const auto copyOut = [&] {
        if (wait_mode & SCE_KERNEL_MSG_PIPE_MODE_DONT_REMOVE) {
            return msgpipe->data_buffer.Peek(recv_buf, msg_size);
        } else {
            return msgpipe->data_buffer.Remove(recv_buf, msg_size);
        }
    };

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);
    std::unique_lock msgpipe_lock(msgpipe->mutex);

    const auto wakeup_senders = [&] {
        if (!msgpipe->senders->empty()) {
            for (auto it = msgpipe->senders->begin(); it != msgpipe->senders->end(); it++) {
                auto threadInfo = (*it);
                if (threadInfo.mp.request_size <= msgpipe->data_buffer.Free()) { // Found a thread we can service
                    threadInfo.thread->status = ThreadStatus::run;
                    threadInfo.thread->status_cond.notify_one();

                    msgpipe->senders->erase(it); // Erase other thread's info - done here to avoid race
                    break; // Should we try to signal other threads, too?
                }
            }
        }
    };

    std::size_t availableSize = msgpipe->data_buffer.Used();
    if ((availableSize >= msg_size) || (ASAP && availableSize >= 1)) { // Copy out and return.
        SceSize copied_size = (SceSize)copyOut();
        wakeup_senders();
        return copied_size;
    } else { // sleep until we can insert
        WaitingThreadData wait_data;
        wait_data.thread = thread;
        wait_data.priority = thread->priority;
        wait_data.mp.request_size = (ASAP) ? 1 : msg_size; // If ASAP, we can read as low as 1 byte

        msgpipe->receivers->push(wait_data);

        std::unique_lock thread_lock(thread->mutex); // Lock thread - needed for condition variable
        thread->update_status(ThreadStatus::wait, ThreadStatus::run); // Mark ourselves as sleeping

        const auto finish = [&] {
            msgpipe_lock.lock(); // Lock message pipe again
            thread->update_status(ThreadStatus::run, ThreadStatus::wait); // Wake up

            SceSize readSize = (SceSize)copyOut();
            // msgpipe->receivers->erase(wait_data); //we've already been erased by the sender
            wakeup_senders();
            return (int)readSize;
        };

        msgpipe_lock.unlock(); // Unlock message pipe object, else we'll deadlock
        if (!timeout) { // No timeout - loop forever until we can fill the buffer
            do {
                // FIXME sleep on SimpleEvent
                thread->status_cond.wait(thread_lock, [&] { return thread->status == ThreadStatus::run; });

                availableSize = msgpipe->data_buffer.Used();
            } while ((availableSize < msg_size) && (ASAP && (availableSize < 1)) && !msgpipe->beingDeleted);

            if (msgpipe->beingDeleted) {
                std::atomic_fetch_add(&msgpipe->remainingThreads, -1);
                return SCE_KERNEL_ERROR_WAIT_DELETE;
            }

            return finish();
        } else { // There's a timeout - wait until we can fill buffer or timeout
            auto status = thread->status_cond.wait_for(thread_lock, std::chrono::microseconds{ *timeout }, [&] { return thread->status == ThreadStatus::run; });
            if (msgpipe->beingDeleted) {
                std::atomic_fetch_add(&msgpipe->remainingThreads, -1);
                return SCE_KERNEL_ERROR_WAIT_DELETE;
            }

            if (!status) { // Timed out and buffer hasn't been touched
                thread->update_status(ThreadStatus::run, ThreadStatus::wait);
                return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
            }

            return finish();
        }
    }
}

// FIXME this should be SendVector!
int msgpipe_send(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID msgpipe_id, SceUInt32 wait_mode, char *send_buf, SceSize msg_size, SceUInt32 *timeout) {
    assert(msgpipe_id >= 0);

    const bool ASAP = !(wait_mode & SCE_KERNEL_MSG_PIPE_MODE_FULL);

    const MsgPipePtr msgpipe = lock_and_find(msgpipe_id, kernel.msgpipes, kernel.mutex);
    if (!msgpipe) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" pipe attr: {} wait_mode: {:#b}"
                  " senders: {} receivers: {}",
            export_name, msgpipe->uid, thread_id, msgpipe->name, msgpipe->attr, wait_mode,
            msgpipe->senders->size(), msgpipe->receivers->size());
    }

    if (msg_size > msgpipe->data_buffer.Capacity())
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_SIZE);

    const auto wakeup_receivers = [&] { // TODO is this correct?
        if (!msgpipe->receivers->empty()) {
            for (auto it = msgpipe->receivers->begin(); it != msgpipe->receivers->end(); it++) {
                if ((*it).mp.request_size <= msgpipe->data_buffer.Used()) { // Found a thread we can service
                    (*it).thread->update_status(ThreadStatus::run, ThreadStatus::wait);

                    msgpipe->receivers->erase(it); // Erase other thread's info - done here to avoid race
                    break; // Should we try to signal other threads, too?
                }
            }
        }
    };

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);
    std::unique_lock<std::mutex> msgpipe_lock(msgpipe->mutex);

    // FIXME implement SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT (for now, all requests are handled synchronously)

    // If ASAP and there's at least 1 free byte, or FULL and there's enough space, copy and return directly.
    std::size_t freeSize = msgpipe->data_buffer.Free();
    if ((freeSize >= msg_size) || (ASAP && (freeSize >= 1))) {
        SceSize copied_size = (SceSize)msgpipe->data_buffer.Insert(send_buf, msg_size);

        wakeup_receivers();

        return copied_size;
    } else { // Go to sleep until there's more space
        WaitingThreadData wait_data;
        wait_data.thread = thread;
        wait_data.priority = thread->priority;
        wait_data.mp.request_size = (ASAP) ? 1 : msg_size; // If ASAP, we can insert as low as 1 byte

        msgpipe->senders->push(wait_data);

        std::unique_lock thread_lock(thread->mutex); // Lock thread - needed for condition variable
        thread->update_status(ThreadStatus::wait, ThreadStatus::run); // Mark ourselves as sleeping

        const auto finish = [&] {
            msgpipe_lock.lock(); // Lock message pipe again
            thread->update_status(ThreadStatus::run, ThreadStatus::wait); // Wake up

            SceSize insertedSize = (SceSize)msgpipe->data_buffer.Insert(send_buf, msg_size);
            // msgpipe->senders->erase(wait_data); //Don't erase ourselves - recv will do it
            wakeup_receivers();
            return (int)insertedSize;
        };

        msgpipe_lock.unlock(); // Unlock message pipe object, else we'll deadlock
        if (!timeout) { // No timeout - loop forever until we can fill the buffer
            do {
                // FIXME sleep on SimpleEvent
                thread->status_cond.wait(thread_lock, [&] { return thread->status == ThreadStatus::run; });

                freeSize = msgpipe->data_buffer.Free();
            } while (((freeSize >= msg_size) || (ASAP && (freeSize >= 1))) || msgpipe->beingDeleted);

            if (msgpipe->beingDeleted) {
                std::atomic_fetch_add(&msgpipe->remainingThreads, -1);
                return SCE_KERNEL_ERROR_WAIT_DELETE;
            }

            return finish();
        } else { // There's a timeout - wait until we can fill buffer or timeout
            auto status = thread->status_cond.wait_for(thread_lock, std::chrono::microseconds{ *timeout }, [&] { return thread->status == ThreadStatus::run; });
            if (msgpipe->beingDeleted) {
                std::atomic_fetch_add(&msgpipe->remainingThreads, -1);
                return SCE_KERNEL_ERROR_WAIT_DELETE;
            }

            if (!status) { // Timed out and buffer hasn't been touched
                thread->update_status(ThreadStatus::run, ThreadStatus::wait);
                return RET_ERROR(SCE_KERNEL_ERROR_WAIT_TIMEOUT);
            }

            return finish();
        }
    }
}

SceUID msgpipe_delete(KernelState &kernel, const char *export_name, const char *name, SceUID thread_id, SceUID msgpipe_id) {
    assert(msgpipe_id >= 0);

    const MsgPipePtr msgpipe = lock_and_find(msgpipe_id, kernel.msgpipes, kernel.mutex);
    if (!msgpipe) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("{}: uid: {} thread_id: {} name: \"{}\" attr: {}",
            export_name, msgpipe->uid, thread_id, msgpipe->name, msgpipe->attr);
    }

    const std::lock_guard<std::mutex> event_lock(msgpipe->mutex);

    if (msgpipe->receivers->empty() && msgpipe->senders->empty()) {
        const std::lock_guard<std::mutex> kernel_lock(kernel.mutex);
        kernel.msgpipes.erase(msgpipe->uid);
    } else {
        msgpipe->remainingThreads = (msgpipe->senders->size() + msgpipe->receivers->size());
        msgpipe->beingDeleted = true;
        std::atomic_thread_fence(std::memory_order_release);

        // Wake up every thread
        for (auto it = msgpipe->senders->begin(); it != msgpipe->senders->end(); it++) {
            (*it).thread->update_status(ThreadStatus::run, ThreadStatus::wait);
        }
        for (auto it = msgpipe->receivers->begin(); it != msgpipe->receivers->end(); it++) {
            (*it).thread->update_status(ThreadStatus::run, ThreadStatus::wait);
        }
        while (std::atomic_load(&msgpipe->remainingThreads) != 0) // FIXME busy loop bad
            ;
    }

    return SCE_KERNEL_OK;
}
