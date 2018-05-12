// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <kernel/thread/sync_primitives.h>

#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/types.h>

#include <cpu/functions.h>
#include <kernel/state.h>
#include <kernel/types.h>
#include <module/module.h>
#include <util/lock_and_find.h>
#include <util/log.h>

static constexpr bool LOG_SYNC_PRIMITIVES = false;

// *********
// * Mutex *
// *********

inline MutexPtrs &get_mutexes(KernelState &kernel, SyncWeight weight) {
    return weight == SyncWeight::Light ? kernel.lwmutexes : kernel.mutexes;
}

SceUID create_mutex(SceUID *uid_out, KernelState &kernel, const char *export_name, SceUID thread_id, const char *mutex_name, SceUInt attr, int init_count, SyncWeight weight) {
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
    mutex->lock_count = init_count;
    std::copy(mutex_name, mutex_name + KERNELOBJECT_MAX_NAME_LENGTH, mutex->name);
    mutex->attr = attr;
    mutex->owner = nullptr;
    if (init_count > 0) {
        const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);
        mutex->owner = thread;
    }
    const std::lock_guard<std::mutex> lock(kernel.mutex);
    const SceUID uid = kernel.next_uid++;
    auto &mutexes = get_mutexes(kernel, weight);
    mutexes.emplace(uid, mutex);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("Creating {}: uid:{} thread_id:{} name:\"{}\" attr:{} init_count:{}",
            weight == SyncWeight::Light ? "lwmutex" : "mutex", uid, thread_id, mutex_name, attr, init_count);
    }

    if (uid_out) {
        *uid_out = uid;
    }

    return SCE_KERNEL_OK;
}

int lock_mutex(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutex_id, int lock_count, unsigned int *timeout, SyncWeight weight) {
    assert(!timeout);

    auto &mutexes = get_mutexes(kernel, weight);
    // TODO Don't lock twice.
    const MutexPtr mutex = lock_and_find(mutex_id, mutexes, kernel.mutex);
    if (!mutex) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("Locking mutex: uid:{} thread_id:{} name:\"{}\" attr:{} lock_count:{}",
            mutex_id, thread_id, mutex->name, mutex->attr, mutex->lock_count);
    }

    const std::lock_guard<std::mutex> lock(mutex->mutex);

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);

    bool is_recursive = (mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);
    bool is_fifo = (mutex->attr & SCE_KERNEL_ATTR_TH_FIFO);

    if (mutex->lock_count > 0) {
        // Already owned

        if (mutex->owner == thread) {
            // Owned by ourselves

            if (is_recursive) {
                mutex->lock_count += lock_count;
                return SCE_KERNEL_OK;
            } else {
                return SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
            }
        } else {
            // Owned by someone else
            // Sleep thread!

            const std::lock_guard<std::mutex> lock2(thread->mutex);
            assert(thread->to_do == ThreadToDo::run);
            thread->to_do = ThreadToDo::wait;
            mutex->waiting_threads.emplace(thread, lock_count, is_fifo ? 0 : thread->priority);
            stop(*thread->cpu);
        }
    } else {
        // Not owned
        // Take ownership!

        mutex->lock_count += lock_count;
        mutex->owner = thread;
    }

    return SCE_KERNEL_OK;
}

int unlock_mutex(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutex_name, int unlock_count, SyncWeight weight) {
    auto &mutexes = get_mutexes(kernel, weight);
    const MutexPtr mutex = lock_and_find(mutex_name, mutexes, kernel.mutex);
    if (!mutex) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("Unlocking mutex: uid:{} thread_id:{} name:\"{}\" attr:{} lock_count:{}",
            mutex_name, thread_id, mutex->name, mutex->attr, mutex->lock_count, unlock_count);
    }

    const std::lock_guard<std::mutex> lock(mutex->mutex);

    const ThreadStatePtr cur_thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);
    if (cur_thread == mutex->owner) {
        if (unlock_count > mutex->lock_count) {
            return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF);
        }
        mutex->lock_count -= unlock_count;
        if (mutex->lock_count == 0) {
            mutex->owner = nullptr;
            if (mutex->waiting_threads.size() > 0) {
                const auto waiting_thread_data = mutex->waiting_threads.top();
                const auto waiting_thread = waiting_thread_data.thread;
                const auto waiting_lock_count = waiting_thread_data.lock_count;

                assert(waiting_thread->to_do == ThreadToDo::wait);
                waiting_thread->to_do = ThreadToDo::run;
                mutex->waiting_threads.pop();
                mutex->lock_count += waiting_lock_count;
                mutex->owner = waiting_thread;
                waiting_thread->something_to_do.notify_one();
            }
        }
    }

    return SCE_KERNEL_OK;
}

int delete_mutex(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID mutex_name, SyncWeight weight) {
    auto &mutexes = get_mutexes(kernel, weight);
    const MutexPtr mutex = lock_and_find(mutex_name, mutexes, kernel.mutex);
    if (!mutex) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    }
    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("Deleting mutex: uid:{} thread_id:{} name:\"{}\" attr:{} lock_count:{} waiting_threads:{}",
            mutex_name, thread_id, mutex->name, mutex->attr, mutex->lock_count, mutex->waiting_threads.size());
    }

    if (mutex->waiting_threads.empty()) {
        const std::lock_guard<std::mutex> lock(mutex->mutex);
        mutexes.erase(mutex_name);
    } else {
        // TODO:
    }

    return SCE_KERNEL_OK;
}

// **************
// * Sempaphore *
// **************

SceUID create_semaphore(KernelState &kernel, const char *export_name, const char *name, SceUInt attr, int initVal, int maxVal) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const SemaphorePtr semaphore = std::make_shared<Semaphore>();
    const SceUID uid = kernel.get_next_uid();
    semaphore->uid = uid;
    semaphore->val = initVal;
    semaphore->max = maxVal;
    semaphore->attr = attr;
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, semaphore->name);
    const std::lock_guard<std::mutex> lock(kernel.mutex);
    const SceUID uid = kernel.next_uid++;
    kernel.semaphores.emplace(uid, semaphore);

    return uid;
}

int wait_semaphore(KernelState &kernel, const char *export_name, SceUID thread_id, SceUID semaid, int signal, SceUInt *timeout) {
    assert(semaid >= 0);
    assert(signal == 1);
    assert(timeout == nullptr);

    // TODO Don't lock twice.
    const SemaphorePtr semaphore = lock_and_find(semaid, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    const std::lock_guard<std::mutex> lock(semaphore->mutex);

    const ThreadStatePtr thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);

    bool is_fifo = (semaphore->attr & SCE_KERNEL_ATTR_TH_FIFO);

    if (semaphore->val <= 0) {
        const std::lock_guard<std::mutex> lock2(thread->mutex);
        assert(thread->to_do == ThreadToDo::run);
        thread->to_do = ThreadToDo::wait;
        semaphore->waiting_threads.emplace(thread, signal, is_fifo ? 0 : thread->priority);
        stop(*thread->cpu);
    } else {
        semaphore->val -= signal;
    }

    return SCE_KERNEL_OK;
}

int signal_sema(KernelState &kernel, const char *export_name, SceUID semaid, int signal) {
    // TODO Don't lock twice.
    const SemaphorePtr semaphore = lock_and_find(semaid, kernel.semaphores, kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    const std::lock_guard<std::mutex> lock(semaphore->mutex);

    if (semaphore->val + signal > semaphore->max) {
        return RET_ERROR(SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF);
    }
    semaphore->val += signal;

    while (semaphore->val > 0 && semaphore->waiting_threads.size() > 0) {
        const auto waiting_thread_data = semaphore->waiting_threads.top();
        const auto waiting_thread = waiting_thread_data.thread;
        const auto waiting_signal_count = waiting_thread_data.lock_count;

        assert(waiting_thread->to_do == ThreadToDo::wait);
        waiting_thread->to_do = ThreadToDo::run;
        semaphore->waiting_threads.pop();
        semaphore->val -= waiting_signal_count;
        waiting_thread->something_to_do.notify_one();
    }

    return 0;
}
