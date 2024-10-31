// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <util/lock_and_find.h>

#include <chrono>
#include <thread>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceThreadmgr);

inline static uint64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

EXPORT(int, __sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, Ptr<SceKernelCreateLwMutex_opt> opt) {
    TRACY_FUNC(__sceKernelCreateLwMutex, workarea, name, attr, opt);
    assert(name != nullptr);
    assert(opt.get(emuenv.mem)->init_count >= 0);

    auto uid_out = &workarea.get(emuenv.mem)->uid;
    return mutex_create(uid_out, emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr, opt.get(emuenv.mem)->init_count, workarea, SyncWeight::Light);
}

EXPORT(int, _sceKernelCancelEvent) {
    TRACY_FUNC(_sceKernelCancelEvent);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelCancelEventFlag, SceUID event_id, SceUInt pattern, SceUInt32 *num_wait_thread) {
    TRACY_FUNC(_sceKernelCancelEventFlag, event_id, pattern, num_wait_thread);
    return eventflag_cancel(emuenv.kernel, export_name, thread_id, event_id, pattern, num_wait_thread);
}

EXPORT(int, _sceKernelCancelEventWithSetPattern) {
    TRACY_FUNC(_sceKernelCancelEventWithSetPattern);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelMsgPipe) {
    TRACY_FUNC(_sceKernelCancelMsgPipe);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelMutex) {
    TRACY_FUNC(_sceKernelCancelMutex);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelRWLock) {
    TRACY_FUNC(_sceKernelCancelRWLock);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelSema, SceUID semaId, SceInt32 setCount, SceUInt32 *pNumWaitThreads) {
    TRACY_FUNC(_sceKernelCancelSema, semaId, setCount, pNumWaitThreads);
    return semaphore_cancel(emuenv.kernel, export_name, thread_id, semaId, setCount, pNumWaitThreads);
}

EXPORT(int, _sceKernelCancelTimer) {
    TRACY_FUNC(_sceKernelCancelTimer);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, _sceKernelCreateCond, const char *pName, SceUInt32 attr, SceUID mutexId, const SceKernelCondOptParam *pOptParam) {
    TRACY_FUNC(_sceKernelCreateCond, pName, attr, mutexId, pOptParam);
    SceUID uid;

    if (auto error = condvar_create(&uid, emuenv.kernel, export_name, pName, thread_id, attr, mutexId, SyncWeight::Heavy)) {
        return error;
    }

    return uid;
}

EXPORT(SceUID, _sceKernelCreateEventFlag, const char *pName, SceUInt32 attr, SceUInt32 initPattern, const SceKernelEventFlagOptParam *pOptParam) {
    TRACY_FUNC(_sceKernelCreateEventFlag, pName, attr, initPattern, pOptParam);
    return eventflag_create(emuenv.kernel, export_name, thread_id, pName, attr, initPattern);
}

EXPORT(int, _sceKernelCreateLwCond, Ptr<SceKernelLwCondWork> workarea, const char *name, SceUInt attr, Ptr<SceKernelCreateLwCond_opt> opt) {
    TRACY_FUNC(_sceKernelCreateLwCond, workarea, name, attr, opt);
    const auto uid_out = &workarea.get(emuenv.mem)->uid;
    const auto assoc_mutex_uid = opt.get(emuenv.mem)->workarea_mutex.get(emuenv.mem)->uid;

    return condvar_create(uid_out, emuenv.kernel, export_name, name, thread_id, attr, assoc_mutex_uid, SyncWeight::Light);
}

EXPORT(int, _sceKernelCreateMsgPipeWithLR) {
    TRACY_FUNC(_sceKernelCreateMsgPipeWithLR);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateMutex, const char *name, SceUInt attr, int init_count, SceKernelMutexOptParam *opt_param) {
    TRACY_FUNC(_sceKernelCreateMutex, name, attr, init_count, opt_param);
    SceUID uid;

    if (auto error = mutex_create(&uid, emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr, init_count, Ptr<SceKernelLwMutexWork>(0), SyncWeight::Heavy)) {
        return error;
    }
    return uid;
}

EXPORT(SceUID, _sceKernelCreateRWLock, const char *name, SceUInt32 attr, SceKernelMutexOptParam *opt_param) {
    TRACY_FUNC(_sceKernelCreateRWLock, name, attr, opt_param);
    return rwlock_create(emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr);
}

EXPORT(int, _sceKernelCreateSema, const char *name, SceUInt attr, int initVal, Ptr<SceKernelCreateSema_opt> opt) {
    TRACY_FUNC(_sceKernelCreateSema, name, attr, initVal, opt);
    return semaphore_create(emuenv.kernel, export_name, name, thread_id, attr, initVal, opt.get(emuenv.mem)->maxVal);
}

EXPORT(int, _sceKernelCreateSema_16XX, const char *name, SceUInt attr, int initVal, Ptr<SceKernelCreateSema_opt> opt) {
    TRACY_FUNC(_sceKernelCreateSema_16XX, name, attr, initVal, opt);
    return semaphore_create(emuenv.kernel, export_name, name, thread_id, attr, initVal, opt.get(emuenv.mem)->maxVal);
}

EXPORT(SceUID, _sceKernelCreateSimpleEvent, const char *name, SceUInt32 attr, SceUInt32 init_pattern, const SceKernelSimpleEventOptParam *pOptParam) {
    TRACY_FUNC(_sceKernelCreateSimpleEvent, name, attr, init_pattern, pOptParam);
    return simple_event_create(emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr, init_pattern);
}

EXPORT(int, _sceKernelCreateTimer, const char *name, SceUInt32 attr, const uint32_t *opt_params) {
    TRACY_FUNC(_sceKernelCreateTimer, name, attr, opt_params);
    return timer_create(emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr);
}

EXPORT(int, _sceKernelDeleteLwCond, Ptr<SceKernelLwCondWork> workarea) {
    TRACY_FUNC(_sceKernelDeleteLwCond, workarea);
    SceUID lightweight_condition_id = workarea.get(emuenv.mem)->uid;

    return condvar_delete(emuenv.kernel, export_name, thread_id, lightweight_condition_id, SyncWeight::Light);
}

EXPORT(int, _sceKernelDeleteLwMutex, Ptr<SceKernelLwMutexWork> workarea) {
    TRACY_FUNC(_sceKernelDeleteLwMutex, workarea);
    if (!workarea)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    const auto lightweight_mutex_id = workarea.get(emuenv.mem)->uid;

    return mutex_delete(emuenv.kernel, export_name, thread_id, lightweight_mutex_id, SyncWeight::Light);
}

EXPORT(int, _sceKernelExitCallback) {
    TRACY_FUNC(_sceKernelExitCallback);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelGetCallbackInfo, SceUID callbackId, SceKernelCallbackInfo *pInfo) {
    TRACY_FUNC(_sceKernelGetCallbackInfo, callbackId, pInfo);
    const CallbackPtr cb = lock_and_find(callbackId, emuenv.kernel.callbacks, emuenv.kernel.mutex);

    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);

    if (!pInfo)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR); // TODO check result

    if (pInfo->size != sizeof(*pInfo))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    pInfo->callbackId = callbackId;
    strncpy(pInfo->name, cb->get_name().c_str(), KERNELOBJECT_MAX_NAME_LENGTH);
    pInfo->name[KERNELOBJECT_MAX_NAME_LENGTH] = '\0';
    pInfo->attr = 0;
    pInfo->threadId = cb->get_owner_thread_id();
    pInfo->callbackFunc = cb->get_callback_function();
    pInfo->notifyId = cb->get_notifier_id();
    pInfo->notifyArg = cb->get_notify_arg();
    pInfo->pCommon = cb->get_user_common_ptr();

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, _sceKernelGetCondInfo, SceUID condId, Ptr<SceKernelCondInfo> pInfo) {
    TRACY_FUNC(_sceKernelGetCondInfo, condId, pInfo);
    const CondvarPtr condvar = lock_and_find(condId, emuenv.kernel.condvars, emuenv.kernel.mutex);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    SceKernelCondInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    info->condId = condId;
    strncpy(info->name, condvar->name, KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info->attr = condvar->attr;
    info->mutexId = condvar->associated_mutex->uid;
    info->numWaitThreads = condvar->waiting_threads->size();

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, _sceKernelGetEventFlagInfo, SceUID evfId, Ptr<SceKernelEventFlagInfo> pInfo) {
    TRACY_FUNC(_sceKernelGetEventFlagInfo, evfId, pInfo);
    const EventFlagPtr eventflag = lock_and_find(evfId, emuenv.kernel.eventflags, emuenv.kernel.mutex);
    if (!eventflag)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    SceKernelEventFlagInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    info->evfId = evfId;
    strncpy(info->name, eventflag->name, KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info->attr = eventflag->attr;
    info->initPattern = eventflag->flags; // Todo, give only current pattern
    info->currentPattern = eventflag->flags;
    info->numWaitThreads = eventflag->waiting_threads->size();

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetEventInfo) {
    TRACY_FUNC(_sceKernelGetEventInfo);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelGetEventPattern, SceUID event_id, SceUInt32 *get_pattern) {
    TRACY_FUNC(_sceKernelGetEventPattern, event_id, get_pattern);
    const SimpleEventPtr event = lock_and_find(event_id, emuenv.kernel.simple_events, emuenv.kernel.mutex);
    if (!event)
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
    MutexPtr mutex = mutex_get(emuenv.kernel, export_name, thread_id, lightweight_mutex_id, SyncWeight::Light);
    if (mutex) {
        info_data->uid = lightweight_mutex_id;
        strncpy(info_data->name, mutex->name, KERNELOBJECT_MAX_NAME_LENGTH + 1);
        info_data->attr = mutex->attr;
        info_data->pWork = mutex->workarea;
        info_data->initCount = mutex->init_count;
        info_data->currentCount = mutex->lock_count;
        if (mutex->owner == 0) {
            info_data->currentOwnerId = 0;
        } else {
            info_data->currentOwnerId = mutex->owner->id;
        }
        info_data->numWaitThreads = static_cast<SceUInt32>(mutex->waiting_threads->size());
        if (info_size < sizeof(SceKernelLwMutexInfo)) {
            memcpy(info.get(emuenv.mem), &info_data_local, info_size);
        } else {
            info_data->size = sizeof(SceKernelLwMutexInfo);
        }
        return SCE_KERNEL_OK;
    } else {
        return SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
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
    const MutexPtr mutex = lock_and_find(mutexId, emuenv.kernel.mutexes, emuenv.kernel.mutex);
    if (!mutex)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    info_data->mutexId = mutexId;
    strncpy(pInfo->name, mutex->name, KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info_data->attr = mutex->attr;
    info_data->initCount = mutex->init_count;
    info_data->currentCount = mutex->lock_count;
    if (mutex->owner) {
        info_data->currentOwnerId = mutex->owner->id;
    } else {
        info_data->currentOwnerId = 0;
    }
    info_data->numWaitThreads = mutex->waiting_threads->size();
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
    const RWLockPtr rwlock = lock_and_find(rwlockId, emuenv.kernel.rwlocks, emuenv.kernel.mutex);
    if (!rwlock)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    const std::lock_guard<std::mutex> rwlock_lock(rwlock->mutex);
    info->rwLockId = rwlock->uid;
    strncpy(info->name, rwlock->name, KERNELOBJECT_MAX_NAME_LENGTH + 1);
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
        if (rwlock->waiting_threads->size() > 0) {
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
        if (rwlock->waiting_threads->size() == 0) {
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
    const SemaphorePtr semaphore = lock_and_find(semaId, emuenv.kernel.semaphores, emuenv.kernel.mutex);
    if (!semaphore)
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
    strncpy(info->name, semaphore->name, KERNELOBJECT_MAX_NAME_LENGTH + 1);
    info->semaId = semaId;
    info->numWaitThreads = semaphore->waiting_threads->size();

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

EXPORT(int, _sceKernelGetThreadExitStatus) {
    TRACY_FUNC(_sceKernelGetThreadExitStatus);
    return UNIMPLEMENTED();
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

    strncpy(info->name, thread->name.c_str(), KERNELOBJECT_MAX_NAME_LENGTH);
    info->stack = Ptr<void>(thread->stack.get());
    info->stackSize = thread->stack_size;
    info->initPriority = thread->priority; // Todo Give only current priority
    info->currentPriority = thread->priority;
    info->initCpuAffinityMask = thread->affinity_mask; // Todo Give init affinity
    info->currentCpuAffinityMask = thread->affinity_mask;
    info->entry = SceKernelThreadEntry(thread->entry_point);
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
    return mutex_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lwmutexid, lock_count, ptimeout, SyncWeight::Light);
}

EXPORT(int, _sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout) {
    TRACY_FUNC(_sceKernelLockMutex, mutexid, lock_count, timeout);
    return mutex_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, mutexid, lock_count, timeout, SyncWeight::Heavy);
}

EXPORT(SceInt32, _sceKernelLockMutexCB, SceUID mutexId, SceInt32 lockCount, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelLockMutexCB, mutexId, lockCount, pTimeout);
    process_callbacks(emuenv.kernel, thread_id);
    return mutex_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, mutexId, lockCount, pTimeout, SyncWeight::Heavy);
}

EXPORT(SceInt32, _sceKernelLockReadRWLock, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelLockReadRWLock, lock_id, timeout);
    return rwlock_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, timeout, false);
}

EXPORT(SceInt32, _sceKernelLockReadRWLockCB, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelLockReadRWLockCB, lock_id, timeout);
    process_callbacks(emuenv.kernel, thread_id);
    return rwlock_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, timeout, false);
}

EXPORT(SceInt32, _sceKernelLockWriteRWLock, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelLockWriteRWLock, lock_id, timeout);
    return rwlock_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, timeout, true);
}

EXPORT(SceInt32, _sceKernelLockWriteRWLockCB, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelLockWriteRWLockCB, lock_id, timeout);
    process_callbacks(emuenv.kernel, thread_id);
    return rwlock_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, timeout, true);
}

EXPORT(int, _sceKernelPMonThreadGetCounter) {
    TRACY_FUNC(_sceKernelPMonThreadGetCounter);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPollEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
    TRACY_FUNC(_sceKernelPollEvent, event_id, bit_pattern, result_pattern, user_data);
    return simple_event_waitorpoll(emuenv.kernel, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, nullptr, false);
}

EXPORT(int, _sceKernelPollEventFlag, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits) {
    TRACY_FUNC(_sceKernelPollEventFlag, event_id, flags, wait, outBits);
    return eventflag_poll(emuenv.kernel, export_name, thread_id, event_id, flags, wait, outBits);
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
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Any), SyncWeight::Light);
}

EXPORT(int, _sceKernelSignalLwCondAll, Ptr<SceKernelLwCondWork> workarea) {
    TRACY_FUNC(_sceKernelSignalLwCondAll, workarea);
    SceUID condid = workarea.get(emuenv.mem)->uid;
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::All), SyncWeight::Light);
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

    if (thread->status == ThreadStatus::run) {
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
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, condId, pTimeout, SyncWeight::Heavy);
}

EXPORT(SceInt32, _sceKernelWaitCondCB, SceUID condId, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitCondCB, condId, pTimeout);
    process_callbacks(emuenv.kernel, thread_id);
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, condId, pTimeout, SyncWeight::Heavy);
}

EXPORT(SceInt32, _sceKernelWaitEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelWaitEvent, event_id, bit_pattern, result_pattern, user_data, timeout);
    return simple_event_waitorpoll(emuenv.kernel, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, timeout, true);
}

EXPORT(SceInt32, _sceKernelWaitEventCB, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout) {
    TRACY_FUNC(_sceKernelWaitEventCB, event_id, bit_pattern, result_pattern, user_data, timeout);
    process_callbacks(emuenv.kernel, thread_id);
    return simple_event_waitorpoll(emuenv.kernel, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, timeout, false);
}

EXPORT(SceInt32, _sceKernelWaitEventFlag, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitEventFlag, evfId, bitPattern, waitMode, pResultPat, pTimeout);
    return eventflag_wait(emuenv.kernel, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat, pTimeout);
}

EXPORT(SceInt32, _sceKernelWaitEventFlagCB, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitEventFlagCB, evfId, bitPattern, waitMode, pResultPat, pTimeout);
    process_callbacks(emuenv.kernel, thread_id);
    return eventflag_wait(emuenv.kernel, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat, pTimeout);
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
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, cond_id, timeout, SyncWeight::Light);
}

EXPORT(SceInt32, _sceKernelWaitLwCondCB, Ptr<SceKernelLwCondWork> pWork, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitLwCondCB, pWork, pTimeout);
    process_callbacks(emuenv.kernel, thread_id);
    const auto cond_id = pWork.get(emuenv.mem)->uid;
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, cond_id, pTimeout, SyncWeight::Light);
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
    return semaphore_wait(emuenv.kernel, export_name, thread_id, semaId, needCount, pTimeout);
}

EXPORT(SceInt32, _sceKernelWaitSemaCB, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout) {
    TRACY_FUNC(_sceKernelWaitSemaCB, semaId, needCount, pTimeout);
    process_callbacks(emuenv.kernel, thread_id);
    return semaphore_wait(emuenv.kernel, export_name, thread_id, semaId, needCount, pTimeout);
}

EXPORT(int, _sceKernelWaitSignal, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    TRACY_FUNC(_sceKernelWaitSignal, unknown, delay, timeout);
    STUBBED("sceKernelWaitSignal");
    const auto thread = emuenv.kernel.get_thread(thread_id);
    thread->update_status(ThreadStatus::wait);
    thread->signal.wait();
    thread->update_status(ThreadStatus::run);
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelWaitSignalCB, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    TRACY_FUNC(_sceKernelWaitSignalCB, unknown, delay, timeout);
    process_callbacks(emuenv.kernel, thread_id);
    return CALL_EXPORT(_sceKernelWaitSignal, unknown, delay, timeout);
}

static int wait_thread_end(ThreadStatePtr &waiter, ThreadStatePtr &target, int *stat) {
    std::unique_lock<std::mutex> waiter_lock(waiter->mutex);
    {
        const std::unique_lock<std::mutex> thread_lock(target->mutex);
        if (target->status == ThreadStatus::dormant) {
            if (stat != nullptr) {
                *stat = target->returned_value;
            }
            return 0;
        }

        waiter->update_status(ThreadStatus::wait);
        target->waiting_threads.push_back(waiter);
    }
    waiter->status_cond.wait(waiter_lock, [&]() { return waiter->status == ThreadStatus::run; });
    return 0;
}

EXPORT(int, _sceKernelWaitThreadEnd, SceUID thid, int *stat, SceUInt *timeout) {
    TRACY_FUNC(_sceKernelWaitThreadEnd, thid, stat, timeout);
    auto waiter = emuenv.kernel.get_thread(thread_id);
    auto target = emuenv.kernel.get_thread(thid);
    if (!target) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    return wait_thread_end(waiter, target, stat);
}

EXPORT(int, _sceKernelWaitThreadEndCB, SceUID thid, int *stat, SceUInt *timeout) {
    TRACY_FUNC(_sceKernelWaitThreadEndCB, thid, stat, timeout);
    auto waiter = emuenv.kernel.get_thread(thread_id);
    auto target = emuenv.kernel.get_thread(thid);
    if (!target) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    process_callbacks(emuenv.kernel, thread_id);
    return wait_thread_end(waiter, target, stat);
}

EXPORT(SceInt32, sceKernelCancelCallback, SceUID callbackId) {
    TRACY_FUNC(sceKernelCancelCallback, callbackId);
    const CallbackPtr cb = lock_and_find(callbackId, emuenv.kernel.callbacks, emuenv.kernel.mutex);

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
    return process_callbacks(emuenv.kernel, thread_id);
}

EXPORT(int, sceKernelCheckWaitableStatus) {
    TRACY_FUNC(sceKernelCheckWaitableStatus);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelClearEvent, SceUID event_id, SceUInt32 clear_pattern) {
    TRACY_FUNC(sceKernelClearEvent, event_id, clear_pattern);
    return simple_event_clear(emuenv.kernel, export_name, thread_id, event_id, clear_pattern);
}

EXPORT(SceInt32, sceKernelClearEventFlag, SceUID evfId, SceUInt32 bitPattern) {
    TRACY_FUNC(sceKernelClearEventFlag, evfId, bitPattern);
    return eventflag_clear(emuenv.kernel, export_name, evfId, bitPattern);
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

    ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    std::string cb_name = name;
    auto cb = std::make_shared<Callback>(thread_id, thread, cb_name, callbackFunc, pCommon);
    std::lock_guard lock(emuenv.kernel.mutex);
    SceUID cb_uid = emuenv.kernel.get_next_uid();
    emuenv.kernel.callbacks.emplace(cb_uid, cb);
    thread->callbacks.push_back(cb);
    return cb_uid;
}

EXPORT(int, sceKernelCreateThreadForUser, const char *name, SceKernelThreadEntry entry, int init_priority, SceKernelCreateThread_opt *options) {
    TRACY_FUNC(sceKernelCreateThreadForUser, name, entry, init_priority, options);
    if (options->cpu_affinity_mask & ~(SCE_KERNEL_CPU_MASK_USER_ALL | 0x80000)) {
        LOG_DEBUG("cpu_affinity_mask: {}", options->cpu_affinity_mask);
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_CPU_AFFINITY);
    }

    const ThreadStatePtr thread = emuenv.kernel.create_thread(emuenv.mem, name, entry.cast<void>(), init_priority, options->cpu_affinity_mask, options->stack_size, options->option.get(emuenv.mem));
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    return thread->id;
}

static int delay_thread(SceUInt delay_us) {
    if (delay_us == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    std::this_thread::sleep_for(std::chrono::microseconds(delay_us));

    return SCE_KERNEL_OK;
}

static int delay_thread_cb(EmuEnvState &emuenv, SceUID thread_id, SceUInt delay_us) {
    auto start = std::chrono::high_resolution_clock::now(); // Meseaure the time taken to process callbacks
    process_callbacks(emuenv.kernel, thread_id);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    if (delay_us > elapsed.count()) // If we spent less time than requested processing callbacks, sleep the remaining time
        return delay_thread(delay_us - elapsed.count());
    else // Else return directly
        return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDelayThread, SceUInt delay) {
    TRACY_FUNC(sceKernelDelayThread, delay);
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThread200, SceUInt delay) {
    TRACY_FUNC(sceKernelDelayThread200, delay);
    if (delay < 201)
        delay = 201;
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThreadCB, SceUInt delay) {
    TRACY_FUNC(sceKernelDelayThreadCB, delay);
    return delay_thread_cb(emuenv, thread_id, delay);
}

EXPORT(int, sceKernelDelayThreadCB200, SceUInt delay) {
    TRACY_FUNC(sceKernelDelayThreadCB200, delay);
    if (delay < 201)
        delay = 201;
    return delay_thread_cb(emuenv, thread_id, delay);
}

EXPORT(int, sceKernelDeleteCallback, SceUID callbackId) {
    TRACY_FUNC(sceKernelDeleteCallback, callbackId);
    const CallbackPtr cb = lock_and_find(callbackId, emuenv.kernel.callbacks, emuenv.kernel.mutex);
    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);
    auto cb_owner_thread = emuenv.kernel.get_thread(cb->get_owner_thread_id());
    std::lock_guard lock(emuenv.kernel.mutex);
    emuenv.kernel.callbacks.erase(callbackId);
    if (cb_owner_thread) {
        auto &v = cb_owner_thread->callbacks;
        std::erase(v, cb);
    }
    return 0;
}

EXPORT(int, sceKernelDeleteCond, SceUID condition_variable_id) {
    TRACY_FUNC(sceKernelDeleteCond, condition_variable_id);
    return condvar_delete(emuenv.kernel, export_name, thread_id, condition_variable_id, SyncWeight::Heavy);
}

EXPORT(int, sceKernelDeleteEventFlag, SceUID event_id) {
    TRACY_FUNC(sceKernelDeleteEventFlag, event_id);
    return eventflag_delete(emuenv.kernel, export_name, thread_id, event_id);
}

EXPORT(SceInt32, sceKernelDeleteMsgPipe, SceUID msgPipeId) {
    TRACY_FUNC(sceKernelDeleteMsgPipe, msgPipeId);
    return msgpipe_delete(emuenv.kernel, export_name, thread_id, msgPipeId);
}

EXPORT(int, sceKernelDeleteMutex, SceUID mutexid) {
    TRACY_FUNC(sceKernelDeleteMutex, mutexid);
    return mutex_delete(emuenv.kernel, export_name, thread_id, mutexid, SyncWeight::Heavy);
}

EXPORT(SceInt32, sceKernelDeleteRWLock, SceUID lock_id) {
    TRACY_FUNC(sceKernelDeleteRWLock, lock_id);
    return rwlock_delete(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id);
}

EXPORT(int, sceKernelDeleteSema, SceUID semaid) {
    TRACY_FUNC(sceKernelDeleteSema, semaid);
    return semaphore_delete(emuenv.kernel, export_name, thread_id, semaid);
}

EXPORT(int, sceKernelDeleteSimpleEvent, SceUID event_id) {
    TRACY_FUNC(sceKernelDeleteSimpleEvent, event_id);
    return simple_event_delete(emuenv.kernel, export_name, thread_id, event_id);
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
    std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);
    emuenv.kernel.timers.erase(timer_handle);

    return 0;
}

EXPORT(int, sceKernelExitDeleteThread, int status) {
    TRACY_FUNC(sceKernelExitDeleteThread, status);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    thread->exit_delete();

    return status;
}

EXPORT(SceInt32, sceKernelGetCallbackCount, SceUID callbackId) {
    TRACY_FUNC(sceKernelGetCallbackCount, callbackId);
    const CallbackPtr cb = lock_and_find(callbackId, emuenv.kernel.callbacks, emuenv.kernel.mutex);

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

EXPORT(int, sceKernelGetThreadmgrUIDClass) {
    TRACY_FUNC(sceKernelGetThreadmgrUIDClass);
    return UNIMPLEMENTED();
}

EXPORT(uint64_t, sceKernelGetTimerBaseWide, SceUID timer_handle) {
    TRACY_FUNC(sceKernelGetTimerBaseWide, timer_handle);
    const TimerPtr timer_info = lock_and_find(timer_handle, emuenv.kernel.timers, emuenv.kernel.mutex);

    if (!timer_info)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    return timer_info->time;
}

EXPORT(uint64_t, sceKernelGetTimerTimeWide, SceUID timer_handle) {
    TRACY_FUNC(sceKernelGetTimerTimeWide, timer_handle);
    const TimerPtr timer_info = lock_and_find(timer_handle, emuenv.kernel.timers, emuenv.kernel.mutex);

    if (!timer_info)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    return get_current_time() - timer_info->time;
}

EXPORT(SceInt32, sceKernelNotifyCallback, SceUID callbackId, SceInt32 notifyArg) {
    TRACY_FUNC(sceKernelNotifyCallback, callbackId, notifyArg);
    const CallbackPtr cb = lock_and_find(callbackId, emuenv.kernel.callbacks, emuenv.kernel.mutex);
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
    return eventflag_find(emuenv.kernel, export_name, pName);
}

EXPORT(SceUID, sceKernelOpenMsgPipe, const char *pName) {
    TRACY_FUNC(sceKernelOpenMsgPipe, pName);
    return msgpipe_find(emuenv.kernel, export_name, pName);
}

EXPORT(SceUID, sceKernelOpenMutex, const char *pName) {
    TRACY_FUNC(sceKernelOpenMutex, pName);
    return mutex_find(emuenv.kernel, export_name, pName);
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
    return semaphore_find(emuenv.kernel, export_name, pName);
}

EXPORT(int, sceKernelOpenSimpleEvent) {
    TRACY_FUNC(sceKernelOpenSimpleEvent);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelOpenTimer, const char *pName) {
    TRACY_FUNC(sceKernelOpenTimer, pName);
    return timer_find(emuenv.kernel, export_name, pName);
}

EXPORT(int, sceKernelPollSema, SceUID semaid, int32_t needCount) {
    TRACY_FUNC(sceKernelPollSema, semaid, needCount);
    assert(needCount >= 0);
    const SemaphorePtr semaphore = lock_and_find(semaid, emuenv.kernel.semaphores, emuenv.kernel.mutex);
    if (!semaphore) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }
    std::unique_lock<std::mutex> semaphore_lock(semaphore->mutex);
    if (semaphore->val < needCount) {
        return SCE_KERNEL_ERROR_SEMA_ZERO;
    }
    semaphore->val -= needCount;
    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, sceKernelPulseEvent, SceUID event_id, SceUInt32 set_pattern, SceUInt64 user_data) {
    TRACY_FUNC(sceKernelPulseEvent, event_id, set_pattern, user_data);
    return simple_event_setorpulse(emuenv.kernel, export_name, thread_id, event_id, set_pattern, user_data, false);
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
    if (!thread->signal.send()) {
        return SCE_KERNEL_ERROR_ALREADY_SENT;
    }
    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, sceKernelSetEvent, SceUID event_id, SceUInt32 set_pattern, SceUInt64 user_data) {
    TRACY_FUNC(sceKernelSetEvent, event_id, set_pattern, user_data);
    return simple_event_setorpulse(emuenv.kernel, export_name, thread_id, event_id, set_pattern, user_data, true);
}

EXPORT(SceInt32, sceKernelSetEventFlag, SceUID evfId, SceUInt32 bitPattern) {
    TRACY_FUNC(sceKernelSetEventFlag, evfId, bitPattern);
    return eventflag_set(emuenv.kernel, export_name, thread_id, evfId, bitPattern);
}

EXPORT(int, sceKernelSetTimerTimeWide, SceUID timer_handle, SceUInt64 time) {
    TRACY_FUNC(sceKernelSetTimerTimeWide, timer_handle, time);
    const TimerPtr timer_info = lock_and_find(timer_handle, emuenv.kernel.timers, emuenv.kernel.mutex);
    if (!timer_info)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID);

    auto oldTime = timer_info->time;
    timer_info->time = time;

    return oldTime;
}

EXPORT(int, sceKernelSignalCond, SceUID condid) {
    TRACY_FUNC(sceKernelSignalCond, condid);
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Any), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalCondAll, SceUID condid) {
    TRACY_FUNC(sceKernelSignalCondAll, condid);
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::All), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalCondTo, SceUID condid, SceUID thread_target) {
    TRACY_FUNC(sceKernelSignalCondTo, condid, thread_target);
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Specific, thread_target), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalSema, SceUID semaid, int signal) {
    TRACY_FUNC(sceKernelSignalSema, semaid, signal);
    return semaphore_signal(emuenv.kernel, export_name, thread_id, semaid, signal);
}

EXPORT(int, sceKernelStartTimer, SceUID timer_handle) {
    TRACY_FUNC(sceKernelStartTimer, timer_handle);
    return timer_start(emuenv.kernel, export_name, thread_id, timer_handle);
}

EXPORT(int, sceKernelStopTimer, SceUID timer_handle) {
    TRACY_FUNC(sceKernelStopTimer, timer_handle);
    return timer_stop(emuenv.kernel, export_name, thread_id, timer_handle);
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
    return mutex_try_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, mutexid, lock_count, SyncWeight::Heavy);
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
    return mutex_unlock(emuenv.kernel, export_name, thread_id, mutexid, unlock_count, SyncWeight::Heavy);
}

EXPORT(int, sceKernelUnlockReadRWLock, SceUID lock_id) {
    TRACY_FUNC(sceKernelUnlockReadRWLock, lock_id);
    return rwlock_unlock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, false);
}

EXPORT(int, sceKernelUnlockWriteRWLock, SceUID lock_id) {
    TRACY_FUNC(sceKernelUnlockWriteRWLock, lock_id);
    return rwlock_unlock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, true);
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
