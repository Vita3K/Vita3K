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

#include "SceThreadmgr.h"
#include <modules/module_parent.h>

#include <kernel/callback.h>
#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <kernel/types.h>
#include <packages/functions.h>

#include <chrono>
#include <thread>
#include <utility>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceThreadmgr);

inline static uint64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

// Waits on or polls a simple event or a timer, which are both events.
static SceInt32 wait_or_poll_event(EmuEnvState &emuenv, const char *export_name, SceUID thread_id, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout, bool is_wait, bool callbacks) {
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    if (const SimpleEventPtr event = emuenv.kernel.objects.find<SimpleEvent>(event_id))
        return event->wait_or_poll(thread, bit_pattern, result_pattern, user_data, timeout, is_wait, callbacks);
    if (const TimerPtr timer = emuenv.kernel.objects.find<Timer>(event_id))
        return timer->wait_or_poll(thread, result_pattern, user_data, timeout, is_wait, callbacks);
    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
}

EXPORT(int, __sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, Ptr<SceKernelCreateLwMutex_opt> opt) {
    TRACY_FUNC(__sceKernelCreateLwMutex, workarea, name, attr, opt);
    const int init_count = opt.get(emuenv.mem)->init_count;
    if (const SceInt32 error = Mutex::check_create(name, attr, init_count))
        return RET_ERROR(error);

    const auto mutex = std::make_shared<LwMutex>(attr, name, init_count, emuenv.kernel.get_thread(thread_id), workarea);
    const SceUID uid = emuenv.kernel.objects.add(mutex, emuenv.kernel.get_next_uid());
    SceKernelLwMutexWork *workarea_mem = workarea.get(emuenv.mem);
    workarea_mem->lockCount = init_count;
    if (workarea_mem->lockCount)
        workarea_mem->owner = thread_id;
    workarea_mem->attr = attr;
    workarea_mem->uid = uid;
    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, _sceKernelCancelEvent, SceUID eventId, SceUInt32 *pNumWaitThreads) {
    TRACY_FUNC(_sceKernelCancelEvent, eventId, pNumWaitThreads);
    if (const SimpleEventPtr event = emuenv.kernel.objects.find<SimpleEvent>(eventId))
        return event->cancel(pNumWaitThreads);
    // this may also be a timer event
    if (const TimerPtr timer = emuenv.kernel.objects.find<Timer>(eventId)) {
        const SceInt32 result = timer->cancel(pNumWaitThreads);
        // Deleted since the lookup, reported like the other event functions do
        return result == SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID ? SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID : result;
    }
    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
}

EXPORT(SceInt32, _sceKernelCancelEventFlag, SceUID event_id, SceUInt pattern, SceUInt32 *num_wait_thread) {
    TRACY_FUNC(_sceKernelCancelEventFlag, event_id, pattern, num_wait_thread);
    const EventFlagPtr event = emuenv.kernel.objects.find<EventFlag>(event_id);
    if (!event)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    return event->cancel(pattern, num_wait_thread);
}

EXPORT(int, _sceKernelCancelEventWithSetPattern) {
    TRACY_FUNC(_sceKernelCancelEventWithSetPattern);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelCancelMsgPipe, SceUID msgPipeId, SceUInt32 *pNumSendWaitThreads, SceUInt32 *pNumReceiveWaitThreads) {
    TRACY_FUNC(_sceKernelCancelMsgPipe, msgPipeId, pNumSendWaitThreads, pNumReceiveWaitThreads);
    const MsgPipePtr msgpipe = emuenv.kernel.objects.find<MsgPipe>(msgPipeId);
    if (!msgpipe)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);
    return msgpipe->cancel(pNumSendWaitThreads, pNumReceiveWaitThreads);
}

EXPORT(SceInt32, _sceKernelCancelMutex, SceUID mutexId, SceInt32 newCount, SceUInt32 *pNumWaitThreads) {
    TRACY_FUNC(_sceKernelCancelMutex, mutexId, newCount, pNumWaitThreads);
    const MutexPtr mutex = emuenv.kernel.objects.find<HeavyMutex>(mutexId);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    return mutex->cancel(emuenv.kernel.get_thread(thread_id), newCount, pNumWaitThreads);
}

EXPORT(SceInt32, _sceKernelCancelRWLock, SceUID rwLockId, SceUInt32 *pNumReadWaitThreads, SceUInt32 *pNumWriteWaitThreads, SceInt32 flag) {
    TRACY_FUNC(_sceKernelCancelRWLock, rwLockId, pNumReadWaitThreads, pNumWriteWaitThreads, flag);
    const RWLockPtr rwlock = emuenv.kernel.objects.find<RWLock>(rwLockId);
    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    return rwlock->cancel(emuenv.kernel.get_thread(thread_id), pNumReadWaitThreads, pNumWriteWaitThreads, flag);
}

EXPORT(int, _sceKernelCancelSema, SceUID semaId, SceInt32 setCount, SceUInt32 *pNumWaitThreads) {
    TRACY_FUNC(_sceKernelCancelSema, semaId, setCount, pNumWaitThreads);
    const SemaphorePtr semaphore = emuenv.kernel.objects.find<Semaphore>(semaId);
    if (!semaphore)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    return semaphore->cancel(setCount, pNumWaitThreads);
}

EXPORT(SceInt32, _sceKernelCancelTimer, SceUID timerId, SceUInt32 *pNumWaitThreads) {
    TRACY_FUNC(_sceKernelCancelTimer, timerId, pNumWaitThreads);
    const TimerPtr timer = emuenv.kernel.objects.find<Timer>(timerId);
    if (!timer)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);
    return timer->cancel(pNumWaitThreads);
}

EXPORT(SceUID, _sceKernelCreateCond, const char *pName, SceUInt32 attr, SceUID mutexId, const SceKernelCondOptParam *pOptParam) {
    TRACY_FUNC(_sceKernelCreateCond, pName, attr, mutexId, pOptParam);
    if (const SceInt32 error = SyncPrimitive::check_name(pName, attr))
        return RET_ERROR(error);
    const MutexPtr mutex = emuenv.kernel.objects.find<HeavyMutex>(mutexId);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);

    const auto condvar = std::make_shared<HeavyCond>(attr, pName, mutex);
    return emuenv.kernel.objects.add(condvar, emuenv.kernel.get_next_uid());
}

EXPORT(SceUID, _sceKernelCreateEventFlag, const char *pName, SceUInt32 attr, SceUInt32 initPattern, const SceKernelEventFlagOptParam *pOptParam) {
    TRACY_FUNC(_sceKernelCreateEventFlag, pName, attr, initPattern, pOptParam);
    if (const SceInt32 error = SyncPrimitive::check_name(pName, attr))
        return RET_ERROR(error);

    const auto event = std::make_shared<EventFlag>(attr, pName, initPattern);
    return emuenv.kernel.objects.add(event, emuenv.kernel.get_next_uid());
}

EXPORT(int, _sceKernelCreateLwCond, Ptr<SceKernelLwCondWork> workarea, const char *name, SceUInt attr, Ptr<SceKernelCreateLwCond_opt> opt) {
    TRACY_FUNC(_sceKernelCreateLwCond, workarea, name, attr, opt);
    const auto assoc_mutex_uid = opt.get(emuenv.mem)->workarea_mutex.get(emuenv.mem)->uid;

    if (const SceInt32 error = SyncPrimitive::check_name(name, attr))
        return RET_ERROR(error);
    const MutexPtr mutex = emuenv.kernel.objects.find<LwMutex>(assoc_mutex_uid);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID);

    const auto condvar = std::make_shared<LwCond>(attr, name, mutex);
    workarea.get(emuenv.mem)->uid = emuenv.kernel.objects.add(condvar, emuenv.kernel.get_next_uid());
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelCreateMsgPipeWithLR) {
    TRACY_FUNC(_sceKernelCreateMsgPipeWithLR);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateMutex, const char *name, SceUInt attr, int init_count, SceKernelMutexOptParam *opt_param) {
    TRACY_FUNC(_sceKernelCreateMutex, name, attr, init_count, opt_param);
    if (const SceInt32 error = Mutex::check_create(name, attr, init_count))
        return RET_ERROR(error);

    const auto mutex = std::make_shared<HeavyMutex>(attr, name, init_count, emuenv.kernel.get_thread(thread_id));
    return emuenv.kernel.objects.add(mutex, emuenv.kernel.get_next_uid());
}

EXPORT(SceUID, _sceKernelCreateRWLock, const char *name, SceUInt32 attr, SceKernelMutexOptParam *opt_param) {
    TRACY_FUNC(_sceKernelCreateRWLock, name, attr, opt_param);
    if (const SceInt32 error = SyncPrimitive::check_name(name, attr))
        return RET_ERROR(error);

    const auto rwlock = std::make_shared<RWLock>(attr, name);
    return emuenv.kernel.objects.add(rwlock, emuenv.kernel.get_next_uid());
}

EXPORT(int, _sceKernelCreateSema, const char *name, SceUInt attr, int initVal, Ptr<SceKernelCreateSema_opt> opt) {
    TRACY_FUNC(_sceKernelCreateSema, name, attr, initVal, opt);
    if (const SceInt32 error = SyncPrimitive::check_name(name, attr))
        return RET_ERROR(error);

    const auto semaphore = std::make_shared<Semaphore>(attr, name, initVal, opt.get(emuenv.mem)->maxVal);
    return emuenv.kernel.objects.add(semaphore, emuenv.kernel.get_next_uid());
}

EXPORT(int, _sceKernelCreateSema_16XX, const char *name, SceUInt attr, int initVal, Ptr<SceKernelCreateSema_opt> opt) {
    TRACY_FUNC(_sceKernelCreateSema_16XX, name, attr, initVal, opt);
    return CALL_EXPORT(_sceKernelCreateSema, name, attr, initVal, opt);
}

EXPORT(SceUID, _sceKernelCreateSimpleEvent, const char *name, SceUInt32 attr, SceUInt32 init_pattern, const SceKernelSimpleEventOptParam *pOptParam) {
    TRACY_FUNC(_sceKernelCreateSimpleEvent, name, attr, init_pattern, pOptParam);
    if (const SceInt32 error = SyncPrimitive::check_name(name, attr))
        return RET_ERROR(error);

    const auto event = std::make_shared<SimpleEvent>(attr, name, init_pattern);
    return emuenv.kernel.objects.add(event, emuenv.kernel.get_next_uid());
}

EXPORT(int, _sceKernelCreateTimer, const char *name, SceUInt32 attr, const uint32_t *opt_params) {
    TRACY_FUNC(_sceKernelCreateTimer, name, attr, opt_params);
    if (const SceInt32 error = SyncPrimitive::check_name(name, attr))
        return RET_ERROR(error);

    const auto timer = std::make_shared<Timer>(attr, name);
    return emuenv.kernel.objects.add(timer, emuenv.kernel.get_next_uid());
}

EXPORT(int, _sceKernelDeleteLwCond, Ptr<SceKernelLwCondWork> workarea) {
    TRACY_FUNC(_sceKernelDeleteLwCond, workarea);
    SceUID lightweight_condition_id = workarea.get(emuenv.mem)->uid;

    if (!emuenv.kernel.objects.remove<LwCond>(lightweight_condition_id))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID);

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelDeleteLwMutex, Ptr<SceKernelLwMutexWork> workarea) {
    TRACY_FUNC(_sceKernelDeleteLwMutex, workarea);
    if (!workarea)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    const auto lightweight_mutex_id = workarea.get(emuenv.mem)->uid;

    if (!emuenv.kernel.objects.remove<LwMutex>(lightweight_mutex_id))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID);

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelExitCallback) {
    TRACY_FUNC(_sceKernelExitCallback);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelGetCallbackInfo, SceUID callbackId, SceKernelCallbackInfo *pInfo) {
    TRACY_FUNC(_sceKernelGetCallbackInfo, callbackId, pInfo);
    const CallbackPtr cb = emuenv.kernel.objects.find<Callback>(callbackId);

    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);

    if (!pInfo)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR); // TODO check result

    if (pInfo->size != sizeof(*pInfo))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    pInfo->callbackId = callbackId;
    strncpy(pInfo->name, cb->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH);
    pInfo->name[KERNELOBJECT_MAX_NAME_LENGTH] = '\0';
    pInfo->attr = 0;
    pInfo->threadId = cb->thread_id;
    pInfo->callbackFunc = cb->cb_func;
    pInfo->notifyId = cb->get_notifier_id();
    pInfo->notifyArg = cb->get_notify_arg();
    pInfo->pCommon = cb->userdata;

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, _sceKernelGetCondInfo, SceUID condId, Ptr<SceKernelCondInfo> pInfo) {
    TRACY_FUNC(_sceKernelGetCondInfo, condId, pInfo);
    const CondvarPtr condvar = emuenv.kernel.objects.find<HeavyCond>(condId);
    const auto guard = condvar ? condvar->lock() : std::unique_lock<std::mutex>();
    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    SceKernelCondInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    info->condId = condId;
    strncpy(info->name, condvar->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info->attr = condvar->attr;
    info->mutexId = condvar->associated_mutex->uid;
    info->numWaitThreads = condvar->waiters.size();

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, _sceKernelGetEventFlagInfo, SceUID evfId, Ptr<SceKernelEventFlagInfo> pInfo) {
    TRACY_FUNC(_sceKernelGetEventFlagInfo, evfId, pInfo);
    const EventFlagPtr eventflag = emuenv.kernel.objects.find<EventFlag>(evfId);
    const auto guard = eventflag ? eventflag->lock() : std::unique_lock<std::mutex>();
    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    SceKernelEventFlagInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    info->evfId = evfId;
    strncpy(info->name, eventflag->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info->attr = eventflag->attr;
    info->initPattern = eventflag->flags; // Todo, give only current pattern
    info->currentPattern = eventflag->flags;
    info->numWaitThreads = eventflag->waiters.size();

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetEventInfo) {
    TRACY_FUNC(_sceKernelGetEventInfo);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelGetEventPattern, SceUID event_id, SceUInt32 *get_pattern) {
    TRACY_FUNC(_sceKernelGetEventPattern, event_id, get_pattern);
    const SimpleEventPtr event = emuenv.kernel.objects.find<SimpleEvent>(event_id);
    const auto guard = event ? event->lock() : std::unique_lock<std::mutex>();
    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    if (!get_pattern)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    *get_pattern = event->pattern;
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetLwCondInfo) {
    TRACY_FUNC(_sceKernelGetLwCondInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetLwCondInfoById) {
    TRACY_FUNC(_sceKernelGetLwCondInfoById);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetLwMutexInfoById, SceUID lightweight_mutex_id, Ptr<SceKernelLwMutexInfo> info, SceSize size) {
    TRACY_FUNC(_sceKernelGetLwMutexInfoById, lightweight_mutex_id, info, size);
    SceKernelLwMutexInfo *info_data = info.get(emuenv.mem);
    SceSize info_size = info_data->size;
    SceKernelLwMutexInfo info_data_local;
    if (info_size < sizeof(SceKernelLwMutexInfo)) {
        info_data = &info_data_local;
        info_data_local.size = info_size;
    }
    const MutexPtr mutex = emuenv.kernel.objects.find<LwMutex>(lightweight_mutex_id);
    const auto guard = mutex ? mutex->lock() : std::unique_lock<std::mutex>();
    if (guard) {
        info_data->uid = lightweight_mutex_id;
        strncpy(info_data->name, mutex->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
        info_data->attr = mutex->attr;
        info_data->pWork = mutex->workarea;
        info_data->initCount = mutex->init_count;
        info_data->currentCount = mutex->lock_count;
        if (mutex->owner == 0) {
            info_data->currentOwnerId = 0;
        } else {
            info_data->currentOwnerId = mutex->owner->id;
        }
        info_data->numWaitThreads = static_cast<SceUInt32>(mutex->waiters.size());
        if (info_size < sizeof(SceKernelLwMutexInfo)) {
            memcpy(info.get(emuenv.mem), &info_data_local, info_size);
        } else {
            info_data->size = sizeof(SceKernelLwMutexInfo);
        }
        return SCE_KERNEL_OK;
    } else {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID);
    }
}

EXPORT(int, _sceKernelGetMsgPipeInfo) {
    TRACY_FUNC(_sceKernelGetMsgPipeInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetMutexInfo, SceUID mutexId, SceKernelMutexInfo *pInfo) {
    TRACY_FUNC(_sceKernelGetMutexInfo, mutexId, pInfo);
    if (!pInfo)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    SceKernelMutexInfo *info_data = pInfo;
    SceSize info_size = info_data->size;
    SceKernelMutexInfo info_data_local;
    if (info_size < sizeof(*pInfo)) {
        info_data = &info_data_local;
        info_data_local.size = info_size;
    }
    const MutexPtr mutex = emuenv.kernel.objects.find<HeavyMutex>(mutexId);
    const auto guard = mutex ? mutex->lock() : std::unique_lock<std::mutex>();
    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    info_data->mutexId = mutexId;
    strncpy(pInfo->name, mutex->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info_data->attr = mutex->attr;
    info_data->initCount = mutex->init_count;
    info_data->currentCount = mutex->lock_count;
    if (mutex->owner) {
        info_data->currentOwnerId = mutex->owner->id;
    } else {
        info_data->currentOwnerId = 0;
    }
    info_data->numWaitThreads = mutex->waiters.size();
    if (info_size < sizeof(*pInfo)) {
        memcpy(pInfo, &info_data_local, info_size);
    } else {
        info_data->size = sizeof(*pInfo);
    }
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetRWLockInfo, SceUID rwlockId, SceKernelRWLockInfo *info) {
    TRACY_FUNC(_sceKernelGetRWLockInfo, rwlockId, info);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    if (info->size < sizeof(SceKernelRWLockInfo))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    const RWLockPtr rwlock = emuenv.kernel.objects.find<RWLock>(rwlockId);
    const auto guard = rwlock ? rwlock->lock() : std::unique_lock<std::mutex>();
    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    info->rwLockId = rwlock->uid;
    strncpy(info->name, rwlock->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info->attr = rwlock->attr;
    if (rwlock->state == RWLockState::Unlocked) {
        info->lockCount = 0;
        info->writeOwnerId = 0;
        info->numReadWaitThreads = 0;
        info->numWriteWaitThreads = 0;
    } else if (rwlock->state == RWLockState::ReadLocked) {
        int lock_count = 0;
        for (auto &t : rwlock->owners) {
            lock_count += t.second;
        }
        info->lockCount = lock_count;
        info->writeOwnerId = 0;
        info->numReadWaitThreads = 0;
        info->numWriteWaitThreads = 0;
        if (rwlock->waiters.size() > 0) {
            STUBBED("info for rw lock with waiting threads is not implemented");
        }
    } else {
        if (rwlock->owners.size() == 1) {
            int lock_count = 0;
            SceUID owner_id = 0;
            for (auto &t : rwlock->owners) {
                lock_count += t.second;
                owner_id = t.first->id;
            }
            info->lockCount = lock_count;
            info->writeOwnerId = owner_id;
        } else {
            STUBBED("info for locked rw lock is not implemented");
        }
        if (rwlock->waiters.size() == 0) {
            info->numReadWaitThreads = 0;
            info->numWriteWaitThreads = 0;
        } else {
            STUBBED("info for rw lock with waiting threads is not implemented");
        }
    }
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelGetSemaInfo, SceUID semaId, Ptr<SceKernelSemaInfo> pInfo) {
    TRACY_FUNC(_sceKernelGetSemaInfo, semaId, pInfo);
    const SemaphorePtr semaphore = emuenv.kernel.objects.find<Semaphore>(semaId);
    const auto guard = semaphore ? semaphore->lock() : std::unique_lock<std::mutex>();
    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);

    SceKernelSemaInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    info->attr = semaphore->attr;
    info->currentCount = semaphore->val;
    info->initCount = semaphore->init_val;
    info->maxCount = semaphore->max;
    strncpy(info->name, semaphore->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info->semaId = semaId;
    info->numWaitThreads = semaphore->waiters.size();

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetSystemInfo) {
    TRACY_FUNC(_sceKernelGetSystemInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetSystemTime) {
    TRACY_FUNC(_sceKernelGetSystemTime);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadContextForVM, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    TRACY_FUNC(_sceKernelGetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
    STUBBED("Stub");

    const ThreadStatePtr thread = emuenv.kernel.get_thread(threadId);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    const auto context = save_context(*thread->cpu);
    SceKernelThreadCpuRegisterInfo *infoCpu = pCpuRegisterInfo.get(emuenv.mem);
    if (infoCpu) {
        if (infoCpu->size != sizeof(*infoCpu))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

        infoCpu->cpsr = context.cpsr;
        memcpy(infoCpu->reg, context.cpu_registers.data(), 16 * 4);
        infoCpu->sb = 100000; // Todo
        infoCpu->st = 100000; // Todo
        infoCpu->teehbr = 100000; // Todo
        infoCpu->tpidrurw = read_tpidruro(*thread->cpu);
    }

    SceKernelThreadVfpRegisterInfo *infoVfp = pVfpRegisterInfo.get(emuenv.mem);
    if (infoVfp) {
        if (infoVfp->size != sizeof(*infoVfp))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

        infoVfp->fpscr = context.fpscr;
        memcpy(infoVfp->reg, context.fpu_registers.data(), 64 * 4);
    }

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, _sceKernelGetThreadCpuAffinityMask, SceUID thid) {
    TRACY_FUNC(_sceKernelGetThreadCpuAffinityMask, thid);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thid ? thid : thread_id);

    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    if (thread->affinity_mask == 0)
        return SCE_KERNEL_CPU_MASK_USER_ALL;

    return thread->affinity_mask;
}

EXPORT(int, _sceKernelGetThreadEventInfo) {
    TRACY_FUNC(_sceKernelGetThreadEventInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadExitStatus, SceUID thid, SceInt32 *pExitStatus) {
    TRACY_FUNC(_sceKernelGetThreadExitStatus, thid, pExitStatus);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thid ? thid : thread_id);
    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }
    if (thread->status != ThreadStatus::dormant) {
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    }
    if (pExitStatus) {
        *pExitStatus = thread->returned_value;
    }
    return 0;
}

EXPORT(SceInt32, _sceKernelGetThreadInfo, SceUID threadId, Ptr<SceKernelThreadInfo> pInfo) {
    TRACY_FUNC(_sceKernelGetThreadInfo, threadId, pInfo);
    STUBBED("STUB");

    const ThreadStatePtr thread = emuenv.kernel.get_thread(threadId ? threadId : thread_id);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    SceKernelThreadInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    // TODO: SCE_KERNEL_ERROR_ILLEGAL_CONTEXT check

    const std::lock_guard<std::mutex> lock(thread->mutex);
    strncpy(info->name, thread->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH);
    info->stack = Ptr<void>(thread->stack.get());
    info->stackSize = thread->stack_size;
    info->initPriority = thread->priority; // Todo Give only current priority
    info->currentPriority = thread->priority;
    info->initCpuAffinityMask = thread->affinity_mask; // Todo Give init affinity
    info->currentCpuAffinityMask = thread->affinity_mask;
    info->entry = SceKernelThreadEntry(thread->entry_point);
    info->status = std::to_underlying(thread->status);
    info->waitType = thread->wait_target.type;
    info->waitId = thread->wait_target.id;
    if (thread->status == ThreadStatus::dormant) {
        info->exitStatus = thread->returned_value;
    }
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetThreadRunStatus) {
    TRACY_FUNC(_sceKernelGetThreadRunStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimerBase) {
    TRACY_FUNC(_sceKernelGetTimerBase);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimerEventRemainingTime) {
    TRACY_FUNC(_sceKernelGetTimerEventRemainingTime);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimerInfo) {
    TRACY_FUNC(_sceKernelGetTimerInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimerTime) {
    TRACY_FUNC(_sceKernelGetTimerTime);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    TRACY_FUNC(_sceKernelLockLwMutex, workarea, lock_count, ptimeout);
    if (!workarea)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    const auto lwmutexid = workarea.get(emuenv.mem)->uid;
    const MutexPtr mutex = emuenv.kernel.objects.find<LwMutex>(lwmutexid);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID);
    return mutex->acquire(emuenv.mem, emuenv.kernel.get_thread(thread_id), lock_count, ptimeout, false, { SCE_KERNEL_WAITTYPE_LW_MUTEX, lwmutexid }, false);
}

EXPORT(int, _sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout) {
    TRACY_FUNC(_sceKernelLockMutex, mutexid, lock_count, timeout);
    const MutexPtr mutex = emuenv.kernel.objects.find<HeavyMutex>(mutexid);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    return mutex->acquire(emuenv.mem, emuenv.kernel.get_thread(thread_id), lock_count, timeout, false, { SCE_KERNEL_WAITTYPE_MUTEX, mutexid }, false);
}

EXPORT(SceInt32, _sceKernelLockMutexCB, SceUID mutexId, SceInt32 lockCount, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelLockMutexCB, mutexId, lockCount, pTimeout);
    emuenv.kernel.get_thread(thread_id)->process_callbacks();
    const MutexPtr mutex = emuenv.kernel.objects.find<HeavyMutex>(mutexId);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    return mutex->acquire(emuenv.mem, emuenv.kernel.get_thread(thread_id), lockCount, pTimeout, false, { SCE_KERNEL_WAITTYPE_MUTEX, mutexId }, true);
}

EXPORT(SceInt32, _sceKernelLockReadRWLock, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelLockReadRWLock, lock_id, timeout);
    const RWLockPtr rwlock = emuenv.kernel.objects.find<RWLock>(lock_id);
    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    return rwlock->acquire(emuenv.kernel.get_thread(thread_id), false, timeout, false);
}

EXPORT(SceInt32, _sceKernelLockReadRWLockCB, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelLockReadRWLockCB, lock_id, timeout);
    emuenv.kernel.get_thread(thread_id)->process_callbacks();
    const RWLockPtr rwlock = emuenv.kernel.objects.find<RWLock>(lock_id);
    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    return rwlock->acquire(emuenv.kernel.get_thread(thread_id), false, timeout, true);
}

EXPORT(SceInt32, _sceKernelLockWriteRWLock, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelLockWriteRWLock, lock_id, timeout);
    const RWLockPtr rwlock = emuenv.kernel.objects.find<RWLock>(lock_id);
    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    return rwlock->acquire(emuenv.kernel.get_thread(thread_id), true, timeout, false);
}

EXPORT(SceInt32, _sceKernelLockWriteRWLockCB, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelLockWriteRWLockCB, lock_id, timeout);
    emuenv.kernel.get_thread(thread_id)->process_callbacks();
    const RWLockPtr rwlock = emuenv.kernel.objects.find<RWLock>(lock_id);
    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    return rwlock->acquire(emuenv.kernel.get_thread(thread_id), true, timeout, true);
}

EXPORT(int, _sceKernelPMonThreadGetCounter) {
    TRACY_FUNC(_sceKernelPMonThreadGetCounter);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPollEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
    TRACY_FUNC(_sceKernelPollEvent, event_id, bit_pattern, result_pattern, user_data);
    return wait_or_poll_event(emuenv, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, nullptr, false, false);
}

EXPORT(int, _sceKernelPollEventFlag, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits) {
    TRACY_FUNC(_sceKernelPollEventFlag, event_id, flags, wait, outBits);
    const EventFlagPtr event = emuenv.kernel.objects.find<EventFlag>(event_id);
    if (!event)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    return event->wait_or_poll(emuenv.kernel.get_thread(thread_id), flags, wait, outBits, nullptr, false, false);
}

EXPORT(int, _sceKernelPulseEventWithNotifyCallback) {
    TRACY_FUNC(_sceKernelPulseEventWithNotifyCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelReceiveMsgPipeVector) {
    TRACY_FUNC(_sceKernelReceiveMsgPipeVector);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelReceiveMsgPipeVectorCB) {
    TRACY_FUNC(_sceKernelReceiveMsgPipeVectorCB);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelRegisterThreadEventHandler, const char *name, SceUID thread_mask, SceUInt32 mask, sceKernelRegisterThreadEventHandlerOpt *opt) {
    TRACY_FUNC(_sceKernelRegisterThreadEventHandler, name, thread_mask, mask, opt);
    if (!opt)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    if (mask & SCE_KERNEL_THREAD_EVENT_TYPE_START) {
        if (emuenv.kernel.thread_event_start)
            LOG_WARN("Multiple thread handlers are not supported");

        emuenv.kernel.thread_event_start = opt->handler;
        emuenv.kernel.thread_event_start_arg = opt->common;
    }

    if (mask & SCE_KERNEL_THREAD_EVENT_TYPE_END) {
        if (emuenv.kernel.thread_event_end)
            LOG_WARN("Multiple thread handlers are not supported");

        emuenv.kernel.thread_event_end = opt->handler;
        emuenv.kernel.thread_event_end_arg = opt->common;
    }

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelSendMsgPipeVector) {
    TRACY_FUNC(_sceKernelSendMsgPipeVector);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSendMsgPipeVectorCB) {
    TRACY_FUNC(_sceKernelSendMsgPipeVectorCB);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetEventWithNotifyCallback) {
    TRACY_FUNC(_sceKernelSetEventWithNotifyCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetThreadContextForVM, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    TRACY_FUNC(_sceKernelSetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(threadId);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    SceKernelThreadCpuRegisterInfo *infoCpu = pCpuRegisterInfo.get(emuenv.mem);
    if (infoCpu) {
        if (infoCpu->size != sizeof(*infoCpu))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

        // Todo
    }

    SceKernelThreadVfpRegisterInfo *infoVfp = pVfpRegisterInfo.get(emuenv.mem);
    if (infoVfp) {
        if (infoVfp->size != sizeof(*infoVfp))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

        // Todo
    }

    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetTimerEvent) {
    TRACY_FUNC(_sceKernelSetTimerEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetTimerTime) {
    TRACY_FUNC(_sceKernelSetTimerTime);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSignalLwCond, Ptr<SceKernelLwCondWork> workarea) {
    TRACY_FUNC(_sceKernelSignalLwCond, workarea);
    SceUID condid = workarea.get(emuenv.mem)->uid;
    const CondvarPtr condvar = emuenv.kernel.objects.find<LwCond>(condid);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID);
    return condvar->signal(Condvar::SignalTarget(Condvar::SignalTarget::Type::Any));
}

EXPORT(int, _sceKernelSignalLwCondAll, Ptr<SceKernelLwCondWork> workarea) {
    TRACY_FUNC(_sceKernelSignalLwCondAll, workarea);
    SceUID condid = workarea.get(emuenv.mem)->uid;
    const CondvarPtr condvar = emuenv.kernel.objects.find<LwCond>(condid);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID);
    return condvar->signal(Condvar::SignalTarget(Condvar::SignalTarget::Type::All));
}

EXPORT(int, _sceKernelSignalLwCondTo) {
    TRACY_FUNC(_sceKernelSignalLwCondTo);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelStartThread, SceUID thid, SceSize arglen, Ptr<void> argp) {
    TRACY_FUNC(_sceKernelStartThread, thid, arglen, argp);
    auto thread = emuenv.kernel.get_thread(thid);

    if (!thread) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }

    if (thread->status == ThreadStatus::running) {
        return RET_ERROR(SCE_KERNEL_ERROR_RUNNING);
    }

    const int res = thread->start(arglen, argp, true);
    if (res < 0) {
        return RET_ERROR(res);
    }
    return res;
}

EXPORT(int, _sceKernelTryReceiveMsgPipeVector) {
    TRACY_FUNC(_sceKernelTryReceiveMsgPipeVector);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelTrySendMsgPipeVector) {
    TRACY_FUNC(_sceKernelTrySendMsgPipeVector);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelUnlockLwMutex) {
    TRACY_FUNC(_sceKernelUnlockLwMutex);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelWaitCond, SceUID condId, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitCond, condId, pTimeout);
    const CondvarPtr condvar = emuenv.kernel.objects.find<HeavyCond>(condId);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
    return condvar->wait(emuenv.mem, emuenv.kernel.get_thread(thread_id), pTimeout, false);
}

EXPORT(SceInt32, _sceKernelWaitCondCB, SceUID condId, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitCondCB, condId, pTimeout);
    emuenv.kernel.get_thread(thread_id)->process_callbacks();
    const CondvarPtr condvar = emuenv.kernel.objects.find<HeavyCond>(condId);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
    return condvar->wait(emuenv.mem, emuenv.kernel.get_thread(thread_id), pTimeout, true);
}

EXPORT(SceInt32, _sceKernelWaitEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelWaitEvent, event_id, bit_pattern, result_pattern, user_data, timeout);
    return wait_or_poll_event(emuenv, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, timeout, true, false);
}

EXPORT(SceInt32, _sceKernelWaitEventCB, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelWaitEventCB, event_id, bit_pattern, result_pattern, user_data, timeout);
    emuenv.kernel.get_thread(thread_id)->process_callbacks();
    return wait_or_poll_event(emuenv, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, timeout, true, true);
}

EXPORT(SceInt32, _sceKernelWaitEventFlag, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitEventFlag, evfId, bitPattern, waitMode, pResultPat, pTimeout);
    const EventFlagPtr event = emuenv.kernel.objects.find<EventFlag>(evfId);
    if (!event)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    return event->wait_or_poll(emuenv.kernel.get_thread(thread_id), bitPattern, waitMode, pResultPat, pTimeout, true, false);
}

EXPORT(SceInt32, _sceKernelWaitEventFlagCB, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitEventFlagCB, evfId, bitPattern, waitMode, pResultPat, pTimeout);
    emuenv.kernel.get_thread(thread_id)->process_callbacks();
    const EventFlagPtr event = emuenv.kernel.objects.find<EventFlag>(evfId);
    if (!event)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    return event->wait_or_poll(emuenv.kernel.get_thread(thread_id), bitPattern, waitMode, pResultPat, pTimeout, true, true);
}

EXPORT(int, _sceKernelWaitException) {
    TRACY_FUNC(_sceKernelWaitException);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitExceptionCB) {
    TRACY_FUNC(_sceKernelWaitExceptionCB);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitLwCond, Ptr<SceKernelLwCondWork> workarea, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelWaitLwCond, workarea, timeout);
    const auto cond_id = workarea.get(emuenv.mem)->uid;
    const CondvarPtr condvar = emuenv.kernel.objects.find<LwCond>(cond_id);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID);
    return condvar->wait(emuenv.mem, emuenv.kernel.get_thread(thread_id), timeout, false);
}

EXPORT(SceInt32, _sceKernelWaitLwCondCB, Ptr<SceKernelLwCondWork> pWork, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitLwCondCB, pWork, pTimeout);
    emuenv.kernel.get_thread(thread_id)->process_callbacks();
    const auto cond_id = pWork.get(emuenv.mem)->uid;
    const CondvarPtr condvar = emuenv.kernel.objects.find<LwCond>(cond_id);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID);
    return condvar->wait(emuenv.mem, emuenv.kernel.get_thread(thread_id), pTimeout, true);
}

EXPORT(int, _sceKernelWaitMultipleEvents) {
    TRACY_FUNC(_sceKernelWaitMultipleEvents);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitMultipleEventsCB) {
    TRACY_FUNC(_sceKernelWaitMultipleEventsCB);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelWaitSema, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitSema, semaId, needCount, pTimeout);
    const SemaphorePtr semaphore = emuenv.kernel.objects.find<Semaphore>(semaId);
    if (!semaphore)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    return semaphore->wait(emuenv.kernel.get_thread(thread_id), needCount, pTimeout, false);
}

EXPORT(SceInt32, _sceKernelWaitSemaCB, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitSemaCB, semaId, needCount, pTimeout);
    emuenv.kernel.get_thread(thread_id)->process_callbacks();
    const SemaphorePtr semaphore = emuenv.kernel.objects.find<Semaphore>(semaId);
    if (!semaphore)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    return semaphore->wait(emuenv.kernel.get_thread(thread_id), needCount, pTimeout, true);
}

EXPORT(int, _sceKernelWaitSignal, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    TRACY_FUNC(_sceKernelWaitSignal, unknown, delay, timeout);
    STUBBED("sceKernelWaitSignal");
    const auto thread = emuenv.kernel.get_thread(thread_id);
    return guest_result(thread->wait_for_signal(false));
}

EXPORT(int, _sceKernelWaitSignalCB, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    TRACY_FUNC(_sceKernelWaitSignalCB, unknown, delay, timeout);
    STUBBED("sceKernelWaitSignalCB");
    const auto thread = emuenv.kernel.get_thread(thread_id);
    thread->process_callbacks();
    return guest_result(thread->wait_for_signal(true));
}

EXPORT(int, _sceKernelWaitThreadEnd, SceUID thid, int *stat, SceUInt *timeout) {
    TRACY_FUNC(_sceKernelWaitThreadEnd, thid, stat, timeout);
    auto waiter = emuenv.kernel.get_thread(thread_id);
    auto target = emuenv.kernel.get_thread(thid);
    if (!target) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    return guest_result(target->wait_for_thread_end(waiter, stat, false));
}

EXPORT(int, _sceKernelWaitThreadEndCB, SceUID thid, int *stat, SceUInt *timeout) {
    TRACY_FUNC(_sceKernelWaitThreadEndCB, thid, stat, timeout);
    auto waiter = emuenv.kernel.get_thread(thread_id);
    waiter->process_callbacks();
    auto target = emuenv.kernel.get_thread(thid);
    if (!target) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    return guest_result(target->wait_for_thread_end(waiter, stat, true));
}

EXPORT(SceInt32, sceKernelCancelCallback, SceUID callbackId) {
    TRACY_FUNC(sceKernelCancelCallback, callbackId);
    const CallbackPtr cb = emuenv.kernel.objects.find<Callback>(callbackId);

    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);
    cb->cancel();

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelChangeActiveCpuMask) {
    TRACY_FUNC(sceKernelChangeActiveCpuMask);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelChangeThreadCpuAffinityMask, SceUID thid, SceInt32 affinity_mask) {
    TRACY_FUNC(sceKernelChangeThreadCpuAffinityMask, thid, affinity_mask);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thid ? thid : thread_id);

    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    const SceInt32 old_affinity = thread->affinity_mask;

    if (affinity_mask & ~SCE_KERNEL_CPU_MASK_USER_ALL)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK);

    thread->affinity_mask = affinity_mask;
    thread->tls.get_ptr<int>().get(emuenv.mem)[TLS_CPU_AFFINITY_MASK] = affinity_mask;
    return old_affinity;
}

EXPORT(SceInt32, sceKernelChangeThreadPriority2, SceUID thid, SceInt32 priority) {
    TRACY_FUNC(sceKernelChangeThreadPriority2, thid, priority);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thid ? thid : thread_id);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    const SceInt32 old_priority = thread->priority;

    if (priority == SCE_KERNEL_CURRENT_THREAD_PRIORITY) {
        priority = emuenv.kernel.get_thread(thread_id)->priority;
    }

    if (priority >= SCE_KERNEL_HIGHEST_DEFAULT_PRIORITY
        && priority <= SCE_KERNEL_LOWEST_DEFAULT_PRIORITY) {
        priority = SCE_KERNEL_GAME_DEFAULT_PRIORITY_ACTUAL + (priority - SCE_KERNEL_DEFAULT_PRIORITY);
    }

    if (priority < SCE_KERNEL_HIGHEST_PRIORITY_USER || priority > SCE_KERNEL_LOWEST_PRIORITY_USER)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_PRIORITY);

    thread->priority = priority;
    thread->tls.get_ptr<int>().get(emuenv.mem)[TLS_CURRENT_PRIORITY] = priority;

    return old_priority;
}

EXPORT(SceInt32, sceKernelChangeThreadPriority, SceUID thid, SceInt32 priority) {
    TRACY_FUNC(sceKernelChangeThreadPriority, thid, priority);
    auto err = CALL_EXPORT(sceKernelChangeThreadPriority2, thid, priority);
    if (err < 0)
        return err;

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelChangeThreadVfpException, SceInt32 clearMask, SceInt32 setMask) {
    TRACY_FUNC(sceKernelChangeThreadVfpException, clearMask, setMask);
    if (((clearMask | setMask) & 0xf7ffff60) != 0 || (clearMask & setMask) != 0) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    int &vfp_exception = thread->tls.get_ptr<int>().get(emuenv.mem)[TLS_VFP_EXCEPTION];
    int old_exception = vfp_exception;
    vfp_exception = setMask | (vfp_exception & ~clearMask);
    STUBBED("");
    return old_exception;
}

EXPORT(SceInt32, sceKernelCheckCallback) {
    TRACY_FUNC(sceKernelCheckCallback);
    return emuenv.kernel.get_thread(thread_id)->process_callbacks();
}

EXPORT(int, sceKernelCheckWaitableStatus) {
    TRACY_FUNC(sceKernelCheckWaitableStatus);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelClearEvent, SceUID event_id, SceUInt32 clear_pattern) {
    TRACY_FUNC(sceKernelClearEvent, event_id, clear_pattern);
    if (const SimpleEventPtr event = emuenv.kernel.objects.find<SimpleEvent>(event_id))
        return event->clear(clear_pattern);
    // this may also be a timer event
    if (const TimerPtr timer = emuenv.kernel.objects.find<Timer>(event_id))
        return timer->clear();
    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
}

EXPORT(SceInt32, sceKernelClearEventFlag, SceUID evfId, SceUInt32 bitPattern) {
    TRACY_FUNC(sceKernelClearEventFlag, evfId, bitPattern);
    const EventFlagPtr event = emuenv.kernel.objects.find<EventFlag>(evfId);
    if (!event)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    return event->clear(bitPattern);
}

EXPORT(int, sceKernelCloseCond) {
    TRACY_FUNC(sceKernelCloseCond);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseEventFlag, SceUID evfId) {
    TRACY_FUNC(sceKernelCloseEventFlag, evfId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseMsgPipe) {
    TRACY_FUNC(sceKernelCloseMsgPipe);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseMutex) {
    TRACY_FUNC(sceKernelCloseMutex);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseMutex_089) {
    TRACY_FUNC(sceKernelCloseMutex_089);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseRWLock) {
    TRACY_FUNC(sceKernelCloseRWLock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseSema) {
    TRACY_FUNC(sceKernelCloseSema);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseSimpleEvent) {
    TRACY_FUNC(sceKernelCloseSimpleEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseTimer) {
    TRACY_FUNC(sceKernelCloseTimer);
    return STUBBED("References not implemented.");
}

EXPORT(SceUID, sceKernelCreateCallback, char *name, SceUInt32 attr, Ptr<SceKernelCallbackFunction> callbackFunc, Ptr<void> pCommon) {
    TRACY_FUNC(sceKernelCreateCallback, name, attr, callbackFunc, pCommon);
    if (attr || !callbackFunc.address())
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ATTR);

    return emuenv.kernel.create_callback(emuenv.kernel.get_thread(thread_id), name, callbackFunc, pCommon);
}

EXPORT(int, sceKernelCreateThreadForUser, const char *name, SceKernelThreadEntry entry, int init_priority, SceKernelCreateThread_opt *options) {
    TRACY_FUNC(sceKernelCreateThreadForUser, name, entry, init_priority, options);
    if (options->cpu_affinity_mask & ~SCE_KERNEL_CPU_MASK_USER_ALL) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_CPU_AFFINITY);
    }

    const ThreadStatePtr thread = emuenv.kernel.create_thread(emuenv.mem, name, entry.cast<void>(), init_priority, options->cpu_affinity_mask, options->stack_size, options->option.get(emuenv.mem));
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    return thread->id;
}

static int delay_thread(KernelState &kernel, SceUID thread_id, SceUInt delay_us) {
    if (delay_us == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    const Deadline deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(delay_us);
    return guest_result(thread->delay_until(deadline, false));
}

static int delay_thread_cb(KernelState &kernel, SceUID thread_id, SceUInt delay_us) {
    // Time spent in callbacks counts toward the delay
    const Deadline deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(delay_us);
    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    return guest_result(thread->delay_until(deadline, true));
}

EXPORT(int, sceKernelDelayThread, SceUInt delay) {
    TRACY_FUNC(sceKernelDelayThread, delay);
    return delay_thread(emuenv.kernel, thread_id, delay);
}

EXPORT(int, sceKernelDelayThread200, SceUInt delay) {
    TRACY_FUNC(sceKernelDelayThread200, delay);
    if (delay < 201)
        delay = 201;
    return delay_thread(emuenv.kernel, thread_id, delay);
}

EXPORT(int, sceKernelDelayThreadCB, SceUInt delay) {
    TRACY_FUNC(sceKernelDelayThreadCB, delay);
    return delay_thread_cb(emuenv.kernel, thread_id, delay);
}

EXPORT(int, sceKernelDelayThreadCB200, SceUInt delay) {
    TRACY_FUNC(sceKernelDelayThreadCB200, delay);
    if (delay < 201)
        delay = 201;
    return delay_thread_cb(emuenv.kernel, thread_id, delay);
}

EXPORT(int, sceKernelDeleteCallback, SceUID callbackId) {
    TRACY_FUNC(sceKernelDeleteCallback, callbackId);
    if (!emuenv.kernel.delete_callback(callbackId))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);
    return 0;
}

EXPORT(int, sceKernelDeleteCond, SceUID condition_variable_id) {
    TRACY_FUNC(sceKernelDeleteCond, condition_variable_id);
    if (!emuenv.kernel.objects.remove<HeavyCond>(condition_variable_id))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_COND_ID);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDeleteEventFlag, SceUID event_id) {
    TRACY_FUNC(sceKernelDeleteEventFlag, event_id);
    if (!emuenv.kernel.objects.remove<EventFlag>(event_id))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, sceKernelDeleteMsgPipe, SceUID msgPipeId) {
    TRACY_FUNC(sceKernelDeleteMsgPipe, msgPipeId);
    if (!emuenv.kernel.objects.remove<MsgPipe>(msgPipeId))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MSG_PIPE_ID);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDeleteMutex, SceUID mutexid) {
    TRACY_FUNC(sceKernelDeleteMutex, mutexid);
    if (!emuenv.kernel.objects.remove<HeavyMutex>(mutexid))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, sceKernelDeleteRWLock, SceUID lock_id) {
    TRACY_FUNC(sceKernelDeleteRWLock, lock_id);
    if (!emuenv.kernel.objects.remove<RWLock>(lock_id))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDeleteSema, SceUID semaid) {
    TRACY_FUNC(sceKernelDeleteSema, semaid);
    if (!emuenv.kernel.objects.remove<Semaphore>(semaid))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDeleteSimpleEvent, SceUID event_id) {
    TRACY_FUNC(sceKernelDeleteSimpleEvent, event_id);
    if (!emuenv.kernel.objects.remove<SimpleEvent>(event_id))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDeleteThread, SceUID thid) {
    TRACY_FUNC(sceKernelDeleteThread, thid);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thid);
    if (!thread || thread->status != ThreadStatus::dormant) {
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    }
    thread->exit_delete(false);
    return 0;
}

EXPORT(int, sceKernelDeleteTimer, SceUID timer_handle) {
    TRACY_FUNC(sceKernelDeleteTimer, timer_handle);
    if (!emuenv.kernel.objects.remove<Timer>(timer_handle))
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelExitDeleteThread, int status) {
    TRACY_FUNC(sceKernelExitDeleteThread, status);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    // Record the exit status for sceKernelWaitThreadEnd, then delete
    thread->exit(status);
    thread->exit_delete();

    return status;
}

EXPORT(SceInt32, sceKernelGetCallbackCount, SceUID callbackId) {
    TRACY_FUNC(sceKernelGetCallbackCount, callbackId);
    const CallbackPtr cb = emuenv.kernel.objects.find<Callback>(callbackId);

    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);

    return cb->get_num_notifications();
}

EXPORT(int, sceKernelGetMsgPipeCreatorId) {
    TRACY_FUNC(sceKernelGetMsgPipeCreatorId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessId) {
    TRACY_FUNC(sceKernelGetProcessId);
    STUBBED("pid: 1");
    return 1;
}

EXPORT(uint64_t, sceKernelGetSystemTimeWide) {
    TRACY_FUNC(sceKernelGetSystemTimeWide);
    return get_current_time();
}

EXPORT(SceInt32, sceKernelGetThreadCpuAffinityMask, SceUID thid) {
    TRACY_FUNC(sceKernelGetThreadCpuAffinityMask, thid);
    return CALL_EXPORT(_sceKernelGetThreadCpuAffinityMask, thid);
}

EXPORT(int, sceKernelGetThreadStackFreeSize) {
    TRACY_FUNC(sceKernelGetThreadStackFreeSize);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceKernelGetThreadTLSAddr, SceUID thid, int key) {
    TRACY_FUNC(sceKernelGetThreadTLSAddr, thid, key);
    return emuenv.kernel.get_thread_tls_addr(emuenv.mem, thid, key);
}

EXPORT(SceInt32, sceKernelGetThreadmgrUIDClass, SceUID uid) {
    TRACY_FUNC(sceKernelGetThreadmgrUIDClass, uid);
    if (emuenv.kernel.get_thread(uid))
        return std::to_underlying(UidClass::thread);
    if (const std::shared_ptr<KernelObject> obj = emuenv.kernel.objects.find(uid))
        return std::to_underlying(obj->get_uid_class());
    return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_UID);
}

EXPORT(uint64_t, sceKernelGetTimerBaseWide, SceUID timer_handle) {
    TRACY_FUNC(sceKernelGetTimerBaseWide, timer_handle);
    const TimerPtr timer = emuenv.kernel.objects.find<Timer>(timer_handle);
    const auto guard = timer ? timer->lock() : std::unique_lock<std::mutex>();

    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    return timer->time;
}

EXPORT(uint64_t, sceKernelGetTimerTimeWide, SceUID timer_handle) {
    TRACY_FUNC(sceKernelGetTimerTimeWide, timer_handle);
    const TimerPtr timer = emuenv.kernel.objects.find<Timer>(timer_handle);
    const auto guard = timer ? timer->lock() : std::unique_lock<std::mutex>();

    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    return get_current_time() - timer->time;
}

EXPORT(SceInt32, sceKernelNotifyCallback, SceUID callbackId, SceInt32 notifyArg) {
    TRACY_FUNC(sceKernelNotifyCallback, callbackId, notifyArg);
    const CallbackPtr cb = emuenv.kernel.objects.find<Callback>(callbackId);
    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);

    cb->direct_notify(notifyArg);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelOpenCond) {
    TRACY_FUNC(sceKernelOpenCond);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelOpenEventFlag, const char *pName) {
    TRACY_FUNC(sceKernelOpenEventFlag, pName);
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    const auto found = emuenv.kernel.objects.find_if<EventFlag>([=](const EventFlag &evf) {
        return evf.name == pName;
    });
    if (!found)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);

    return found->uid;
}

EXPORT(SceUID, sceKernelOpenMsgPipe, const char *pName) {
    TRACY_FUNC(sceKernelOpenMsgPipe, pName);
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    const auto found = emuenv.kernel.objects.find_if<MsgPipe>([=](const MsgPipe &msg_pipe) {
        return msg_pipe.name == pName;
    });
    if (!found)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);

    return found->uid;
}

EXPORT(SceUID, sceKernelOpenMutex, const char *pName) {
    TRACY_FUNC(sceKernelOpenMutex, pName);
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    const auto found = emuenv.kernel.objects.find_if<HeavyMutex>([=](const Mutex &mutex) {
        return mutex.name == pName;
    });
    if (!found)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);

    return found->uid;
}

EXPORT(int, sceKernelOpenMutex_089) {
    TRACY_FUNC(sceKernelOpenMutex_089);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenRWLock) {
    TRACY_FUNC(sceKernelOpenRWLock);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelOpenSema, const char *pName) {
    TRACY_FUNC(sceKernelOpenSema, pName);
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    const auto found = emuenv.kernel.objects.find_if<Semaphore>([=](const Semaphore &sema) {
        return sema.name == pName;
    });
    if (!found)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);

    return found->uid;
}

EXPORT(int, sceKernelOpenSimpleEvent) {
    TRACY_FUNC(sceKernelOpenSimpleEvent);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelOpenTimer, const char *pName) {
    TRACY_FUNC(sceKernelOpenTimer, pName);
    if (strlen(pName) > KERNELOBJECT_MAX_NAME_LENGTH)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);

    const auto found = emuenv.kernel.objects.find_if<Timer>([=](const Timer &timer) {
        return timer.name == pName;
    });
    if (!found)
        return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);

    return found->uid;
}

EXPORT(int, sceKernelPollSema, SceUID semaid, int32_t needCount) {
    TRACY_FUNC(sceKernelPollSema, semaid, needCount);
    assert(needCount >= 0);
    const SemaphorePtr semaphore = emuenv.kernel.objects.find<Semaphore>(semaid);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }
    return semaphore->poll(needCount);
}

EXPORT(SceInt32, sceKernelPulseEvent, SceUID event_id, SceUInt32 set_pattern, SceUInt64 user_data) {
    TRACY_FUNC(sceKernelPulseEvent, event_id, set_pattern, user_data);
    const SimpleEventPtr event = emuenv.kernel.objects.find<SimpleEvent>(event_id);
    if (!event)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    return event->set_or_pulse(set_pattern, user_data, false);
}

EXPORT(int, sceKernelRegisterCallbackToEvent) {
    TRACY_FUNC(sceKernelRegisterCallbackToEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelResumeThreadForVM, SceUID threadId) {
    TRACY_FUNC(sceKernelResumeThreadForVM, threadId);
    STUBBED("STUB");

    const ThreadStatePtr thread = emuenv.kernel.get_thread(threadId);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    thread->resume();

    return 0;
}

EXPORT(int, sceKernelSendSignal, SceUID target_thread_id) {
    TRACY_FUNC(sceKernelSendSignal, target_thread_id);
    STUBBED("sceKernelSendSignal");
    const auto thread = emuenv.kernel.get_thread(target_thread_id);
    return thread->send_signal();
}

EXPORT(SceInt32, sceKernelSetEvent, SceUID event_id, SceUInt32 set_pattern, SceUInt64 user_data) {
    TRACY_FUNC(sceKernelSetEvent, event_id, set_pattern, user_data);
    const SimpleEventPtr event = emuenv.kernel.objects.find<SimpleEvent>(event_id);
    if (!event)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID);
    return event->set_or_pulse(set_pattern, user_data, true);
}

EXPORT(SceInt32, sceKernelSetEventFlag, SceUID evfId, SceUInt32 bitPattern) {
    TRACY_FUNC(sceKernelSetEventFlag, evfId, bitPattern);
    const EventFlagPtr event = emuenv.kernel.objects.find<EventFlag>(evfId);
    if (!event)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);
    return event->set(bitPattern);
}

EXPORT(int, sceKernelSetTimerTimeWide, SceUID timer_handle, SceUInt64 time) {
    TRACY_FUNC(sceKernelSetTimerTimeWide, timer_handle, time);
    const TimerPtr timer = emuenv.kernel.objects.find<Timer>(timer_handle);
    const auto guard = timer ? timer->lock() : std::unique_lock<std::mutex>();
    if (!guard)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    auto oldTime = timer->time;
    timer->time = time;

    return oldTime;
}

EXPORT(int, sceKernelSignalCond, SceUID condid) {
    TRACY_FUNC(sceKernelSignalCond, condid);
    const CondvarPtr condvar = emuenv.kernel.objects.find<HeavyCond>(condid);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
    return condvar->signal(Condvar::SignalTarget(Condvar::SignalTarget::Type::Any));
}

EXPORT(int, sceKernelSignalCondAll, SceUID condid) {
    TRACY_FUNC(sceKernelSignalCondAll, condid);
    const CondvarPtr condvar = emuenv.kernel.objects.find<HeavyCond>(condid);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
    return condvar->signal(Condvar::SignalTarget(Condvar::SignalTarget::Type::All));
}

EXPORT(int, sceKernelSignalCondTo, SceUID condid, SceUID thread_target) {
    TRACY_FUNC(sceKernelSignalCondTo, condid, thread_target);
    const CondvarPtr condvar = emuenv.kernel.objects.find<HeavyCond>(condid);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
    return condvar->signal(Condvar::SignalTarget(Condvar::SignalTarget::Type::Specific, thread_target));
}

EXPORT(int, sceKernelSignalSema, SceUID semaid, int signal) {
    TRACY_FUNC(sceKernelSignalSema, semaid, signal);
    const SemaphorePtr semaphore = emuenv.kernel.objects.find<Semaphore>(semaid);
    if (!semaphore)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    return semaphore->signal(signal);
}

EXPORT(int, sceKernelStartTimer, SceUID timer_handle) {
    TRACY_FUNC(sceKernelStartTimer, timer_handle);
    const TimerPtr timer = emuenv.kernel.objects.find<Timer>(timer_handle);
    if (!timer)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);
    return timer->start();
}

EXPORT(int, sceKernelStopTimer, SceUID timer_handle) {
    TRACY_FUNC(sceKernelStopTimer, timer_handle);
    const TimerPtr timer = emuenv.kernel.objects.find<Timer>(timer_handle);
    if (!timer)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);
    return timer->stop();
}

EXPORT(int, sceKernelSuspendThreadForVM, SceUID threadId) {
    TRACY_FUNC(sceKernelSuspendThreadForVM, threadId);
    STUBBED("STUB");

    const ThreadStatePtr thread = emuenv.kernel.get_thread(threadId);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    thread->suspend();

    return 0;
}

EXPORT(int, sceKernelTryLockMutex, SceUID mutexid, int lock_count) {
    TRACY_FUNC(sceKernelTryLockMutex, mutexid, lock_count);
    const MutexPtr mutex = emuenv.kernel.objects.find<HeavyMutex>(mutexid);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    // Never waits, so it has no wait target
    return mutex->acquire(emuenv.mem, emuenv.kernel.get_thread(thread_id), lock_count, nullptr, true, {}, false);
}

EXPORT(int, sceKernelTryLockReadRWLock) {
    TRACY_FUNC(sceKernelTryLockReadRWLock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryLockWriteRWLock) {
    TRACY_FUNC(sceKernelTryLockWriteRWLock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnlockMutex, SceUID mutexid, int unlock_count) {
    TRACY_FUNC(sceKernelUnlockMutex, mutexid, unlock_count);
    const MutexPtr mutex = emuenv.kernel.objects.find<HeavyMutex>(mutexid);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    return mutex->release(emuenv.kernel.get_thread(thread_id), unlock_count);
}

EXPORT(int, sceKernelUnlockReadRWLock, SceUID lock_id) {
    TRACY_FUNC(sceKernelUnlockReadRWLock, lock_id);
    const RWLockPtr rwlock = emuenv.kernel.objects.find<RWLock>(lock_id);
    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    return rwlock->release(emuenv.kernel.get_thread(thread_id));
}

EXPORT(int, sceKernelUnlockWriteRWLock, SceUID lock_id) {
    TRACY_FUNC(sceKernelUnlockWriteRWLock, lock_id);
    const RWLockPtr rwlock = emuenv.kernel.objects.find<RWLock>(lock_id);
    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    return rwlock->release(emuenv.kernel.get_thread(thread_id));
}

EXPORT(int, sceKernelUnregisterCallbackFromEvent) {
    TRACY_FUNC(sceKernelUnregisterCallbackFromEvent);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnregisterCallbackFromEventAll) {
    TRACY_FUNC(sceKernelUnregisterCallbackFromEventAll);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnregisterThreadEventHandler) {
    TRACY_FUNC(sceKernelUnregisterThreadEventHandler);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitThreadEndCB_089) {
    TRACY_FUNC(sceKernelWaitThreadEndCB_089);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitThreadEnd_089) {
    TRACY_FUNC(sceKernelWaitThreadEnd_089);
    return UNIMPLEMENTED();
}
