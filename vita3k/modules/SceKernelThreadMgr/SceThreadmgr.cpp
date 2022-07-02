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

#include "SceThreadmgr.h"
#include <modules/module_parent.h>

#include <kernel/callback.h>
#include <kernel/sync_primitives.h>
#include <package/functions.h>

#include <util/lock_and_find.h>

#include <SDL_timer.h>

#include <chrono>
#include <thread>

inline uint64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

EXPORT(int, __sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, Ptr<SceKernelCreateLwMutex_opt> opt) {
    assert(name != nullptr);
    assert(opt.get(emuenv.mem)->init_count >= 0);

    auto uid_out = &workarea.get(emuenv.mem)->uid;
    return mutex_create(uid_out, emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr, opt.get(emuenv.mem)->init_count, workarea, SyncWeight::Light);
}

EXPORT(int, _sceKernelCancelEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelEventWithSetPattern) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCancelTimer) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, _sceKernelCreateCond, const char *pName, SceUInt32 attr, SceUID mutexId, const SceKernelCondOptParam *pOptParam) {
    SceUID uid;

    if (auto error = condvar_create(&uid, emuenv.kernel, export_name, pName, thread_id, attr, mutexId, SyncWeight::Heavy)) {
        return error;
    }

    return uid;
}

EXPORT(SceUID, _sceKernelCreateEventFlag, const char *pName, SceUInt32 attr, SceUInt32 initPattern, const SceKernelEventFlagOptParam *pOptParam) {
    return eventflag_create(emuenv.kernel, export_name, thread_id, pName, attr, initPattern);
}

EXPORT(int, _sceKernelCreateLwCond, Ptr<SceKernelLwCondWork> workarea, const char *name, SceUInt attr, Ptr<SceKernelCreateLwCond_opt> opt) {
    const auto uid_out = &workarea.get(emuenv.mem)->uid;
    const auto assoc_mutex_uid = opt.get(emuenv.mem)->workarea_mutex.get(emuenv.mem)->uid;

    return condvar_create(uid_out, emuenv.kernel, export_name, name, thread_id, attr, assoc_mutex_uid, SyncWeight::Light);
}

EXPORT(int, _sceKernelCreateMsgPipeWithLR) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateMutex, const char *name, SceUInt attr, int init_count, SceKernelMutexOptParam *opt_param) {
    SceUID uid;

    if (auto error = mutex_create(&uid, emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr, init_count, Ptr<SceKernelLwMutexWork>(0), SyncWeight::Heavy)) {
        return error;
    }
    return uid;
}

EXPORT(SceUID, _sceKernelCreateRWLock, const char *name, SceUInt32 attr, SceKernelMutexOptParam *opt_param) {
    return rwlock_create(emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr);
}

EXPORT(int, _sceKernelCreateSema, const char *name, SceUInt attr, int initVal, Ptr<SceKernelCreateSema_opt> opt) {
    return semaphore_create(emuenv.kernel, export_name, name, thread_id, attr, initVal, opt.get(emuenv.mem)->maxVal);
}

EXPORT(int, _sceKernelCreateSema_16XX, const char *name, SceUInt attr, int initVal, Ptr<SceKernelCreateSema_opt> opt) {
    return semaphore_create(emuenv.kernel, export_name, name, thread_id, attr, initVal, opt.get(emuenv.mem)->maxVal);
}

EXPORT(SceUID, _sceKernelCreateSimpleEvent, const char *name, SceUInt32 attr, SceUInt32 init_pattern, const SceKernelSimpleEventOptParam *pOptParam) {
    return simple_event_create(emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr, init_pattern);
}

EXPORT(int, _sceKernelCreateTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelDeleteLwCond, Ptr<SceKernelLwCondWork> workarea) {
    SceUID lightweight_condition_id = workarea.get(emuenv.mem)->uid;

    return condvar_delete(emuenv.kernel, export_name, thread_id, lightweight_condition_id, SyncWeight::Light);
}

EXPORT(int, _sceKernelDeleteLwMutex, Ptr<SceKernelLwMutexWork> workarea) {
    if (!workarea)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    const auto lightweight_mutex_id = workarea.get(emuenv.mem)->uid;

    return mutex_delete(emuenv.kernel, export_name, thread_id, lightweight_mutex_id, SyncWeight::Light);
}

EXPORT(int, _sceKernelExitCallback) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelGetCallbackInfo, SceUID callbackId, SceKernelCallbackInfo *pInfo) {
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
    const CondvarPtr condvar = lock_and_find(condId, emuenv.kernel.condvars, emuenv.kernel.mutex);
    if (!condvar)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    SceKernelCondInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    info->condId = condId;
    std::copy(condvar->name, condvar->name + KERNELOBJECT_MAX_NAME_LENGTH, info->name);
    info->attr = condvar->attr;
    info->mutexId = condvar->associated_mutex->uid;
    info->numWaitThreads = condvar->waiting_threads->size();

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, _sceKernelGetEventFlagInfo, SceUID evfId, Ptr<SceKernelEventFlagInfo> pInfo) {
    const EventFlagPtr eventflag = lock_and_find(evfId, emuenv.kernel.eventflags, emuenv.kernel.mutex);
    if (!eventflag)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    SceKernelEventFlagInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    info->evfId = evfId;
    std::copy(eventflag->name, eventflag->name + KERNELOBJECT_MAX_NAME_LENGTH, info->name);
    info->attr = eventflag->attr;
    info->initPattern = eventflag->flags; // Todo, give only current pattern
    info->currentPattern = eventflag->flags;
    info->numWaitThreads = eventflag->waiting_threads->size();

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetEventInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetEventPattern) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetLwCondInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetLwCondInfoById) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetLwMutexInfoById, SceUID lightweight_mutex_id, Ptr<SceKernelLwMutexInfo> info, SceSize size) {
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
        std::copy(mutex->name, mutex->name + KERNELOBJECT_MAX_NAME_LENGTH, info_data->name);
        info_data->attr = mutex->attr;
        info_data->pWork = mutex->workarea;
        info_data->initCount = mutex->init_count;
        info_data->currentCount = mutex->lock_count;
        if (mutex->owner == 0) {
            info_data->currentOwnerId = 0;
        } else {
            auto workarea_mutex_owner = mutex->workarea.get(emuenv.mem)->owner;
            auto threads = emuenv.kernel.threads;
            if (threads[workarea_mutex_owner] == mutex->owner) { // something like optimisation
                info_data->currentOwnerId = workarea_mutex_owner;
            } else {
                info_data->currentOwnerId = -1;
                for (auto it = threads.begin(); it != threads.end(); ++it) {
                    if (it->second == mutex->owner) {
                        info_data->currentOwnerId = it->first;
                        break;
                    }
                }
            }
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
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetMutexInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetRWLockInfo) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelGetSemaInfo, SceUID semaId, Ptr<SceKernelSemaInfo> pInfo) {
    const SemaphorePtr semaphore = lock_and_find(semaId, emuenv.kernel.semaphores, emuenv.kernel.mutex);
    if (!semaphore)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);

    SceKernelSemaInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    info->attr = semaphore->attr;
    info->currentCount = emuenv.kernel.semaphores.size();
    info->initCount = semaphore->val;
    info->maxCount = semaphore->max;
    std::copy(semaphore->name, semaphore->name + KERNELOBJECT_MAX_NAME_LENGTH, info->name);
    info->semaId = semaId;
    info->numWaitThreads = semaphore->waiting_threads->size();

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetSystemInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetSystemTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadContextForVM, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    STUBBED("Stub");

    const ThreadStatePtr thread = lock_and_find(threadId, emuenv.kernel.threads, emuenv.kernel.mutex);
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
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thid ? thid : thread_id);

    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    return thread->affinity_mask;
}

EXPORT(int, _sceKernelGetThreadEventInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadExitStatus) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelGetThreadInfo, SceUID threadId, Ptr<SceKernelThreadInfo> pInfo) {
    STUBBED("STUB");

    const ThreadStatePtr thread = lock_and_find(threadId ? threadId : thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    SceKernelThreadInfo *info = pInfo.get(emuenv.mem);
    if (!info)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);

    if (info->size != sizeof(*info))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    // TODO: SCE_KERNEL_ERROR_ILLEGAL_CONTEXT check

    std::copy(thread->name.c_str(), thread->name.c_str() + KERNELOBJECT_MAX_NAME_LENGTH, info->name);
    info->stack = Ptr<void>(thread->stack.get());
    info->stackSize = thread->stack_size;
    info->initPriority = thread->priority; // Todo Give only current priority
    info->currentPriority = thread->priority;
    info->initCpuAffinityMask = thread->affinity_mask; // Todo Give init affinity
    info->currentCpuAffinityMask = thread->affinity_mask;
    info->entry = SceKernelThreadEntry(thread->entry_point);

    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelGetThreadRunStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimerBase) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimerEventRemainingTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimerInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetTimerTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    if (!workarea)
        return RET_ERROR(SCE_GXM_ERROR_INVALID_POINTER);

    const auto lwmutexid = workarea.get(emuenv.mem)->uid;
    return mutex_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lwmutexid, lock_count, ptimeout, SyncWeight::Light);
}

EXPORT(int, _sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout) {
    return mutex_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, mutexid, lock_count, timeout, SyncWeight::Heavy);
}

EXPORT(SceInt32, _sceKernelLockMutexCB, SceUID mutexId, SceInt32 lockCount, SceUInt32 *pTimeout) {
    process_callbacks(emuenv.kernel, thread_id);
    return mutex_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, mutexId, lockCount, pTimeout, SyncWeight::Heavy);
}

EXPORT(SceInt32, _sceKernelLockReadRWLock, SceUID lock_id, SceUInt32 *timeout) {
    return rwlock_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, timeout, false);
}

EXPORT(SceInt32, _sceKernelLockReadRWLockCB, SceUID lock_id, SceUInt32 *timeout) {
    process_callbacks(emuenv.kernel, thread_id);
    return rwlock_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, timeout, false);
}

EXPORT(SceInt32, _sceKernelLockWriteRWLock, SceUID lock_id, SceUInt32 *timeout) {
    return rwlock_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, timeout, true);
}

EXPORT(SceInt32, _sceKernelLockWriteRWLockCB, SceUID lock_id, SceUInt32 *timeout) {
    process_callbacks(emuenv.kernel, thread_id);
    return rwlock_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, timeout, true);
}

EXPORT(int, _sceKernelPMonThreadGetCounter) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPollEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
    return simple_event_waitorpoll(emuenv.kernel, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, nullptr, false);
}

EXPORT(int, _sceKernelPollEventFlag, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits) {
    return eventflag_poll(emuenv.kernel, export_name, thread_id, event_id, flags, wait, outBits);
}

EXPORT(int, _sceKernelPulseEventWithNotifyCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelReceiveMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelReceiveMsgPipeVectorCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelRegisterThreadEventHandler) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSendMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSendMsgPipeVectorCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetEventWithNotifyCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetThreadContextForVM, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    const ThreadStatePtr thread = lock_and_find(threadId, emuenv.kernel.threads, emuenv.kernel.mutex);
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
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetTimerTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSignalLwCond, Ptr<SceKernelLwCondWork> workarea) {
    SceUID condid = workarea.get(emuenv.mem)->uid;
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Any), SyncWeight::Light);
}

EXPORT(int, _sceKernelSignalLwCondAll, Ptr<SceKernelLwCondWork> workarea) {
    SceUID condid = workarea.get(emuenv.mem)->uid;
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::All), SyncWeight::Light);
}

EXPORT(int, _sceKernelSignalLwCondTo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelStartThread, SceUID thid, SceSize arglen, Ptr<void> argp) {
    auto thread = lock_and_find(thid, emuenv.kernel.threads, emuenv.kernel.mutex);
    Ptr<void> new_argp(0);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    if (thread->status == ThreadStatus::run) {
        return SCE_KERNEL_ERROR_RUNNING;
    }

    const int res = thread->start(emuenv.kernel, arglen, argp);
    if (res < 0) {
        return RET_ERROR(res);
    }
    return res;
}

EXPORT(int, _sceKernelTryReceiveMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelTrySendMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelUnlockLwMutex) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelWaitCond, SceUID condId, SceUInt32 *pTimeout) {
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, condId, pTimeout, SyncWeight::Heavy);
}

EXPORT(SceInt32, _sceKernelWaitCondCB, SceUID condId, SceUInt32 *pTimeout) {
    process_callbacks(emuenv.kernel, thread_id);
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, condId, pTimeout, SyncWeight::Heavy);
}

EXPORT(SceInt32, _sceKernelWaitEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout) {
    return simple_event_waitorpoll(emuenv.kernel, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, timeout, true);
}

EXPORT(SceInt32, _sceKernelWaitEventCB, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout) {
    process_callbacks(emuenv.kernel, thread_id);
    return simple_event_waitorpoll(emuenv.kernel, export_name, thread_id, event_id, bit_pattern, result_pattern, user_data, timeout, false);
}

EXPORT(SceInt32, _sceKernelWaitEventFlag, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    return eventflag_wait(emuenv.kernel, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat, pTimeout);
}

EXPORT(SceInt32, _sceKernelWaitEventFlagCB, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    process_callbacks(emuenv.kernel, thread_id);
    return eventflag_wait(emuenv.kernel, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat, pTimeout);
}

EXPORT(int, _sceKernelWaitException) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitExceptionCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitLwCond, Ptr<SceKernelLwCondWork> workarea, SceUInt32 *timeout) {
    const auto cond_id = workarea.get(emuenv.mem)->uid;
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, cond_id, timeout, SyncWeight::Light);
}

EXPORT(SceInt32, _sceKernelWaitLwCondCB, Ptr<SceKernelLwCondWork> pWork, SceUInt32 *pTimeout) {
    process_callbacks(emuenv.kernel, thread_id);
    const auto cond_id = pWork.get(emuenv.mem)->uid;
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, cond_id, pTimeout, SyncWeight::Light);
}

EXPORT(int, _sceKernelWaitMultipleEvents) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitMultipleEventsCB) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, _sceKernelWaitSema, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout) {
    return semaphore_wait(emuenv.kernel, export_name, thread_id, semaId, needCount, pTimeout);
}

EXPORT(SceInt32, _sceKernelWaitSemaCB, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout) {
    process_callbacks(emuenv.kernel, thread_id);
    return semaphore_wait(emuenv.kernel, export_name, thread_id, semaId, needCount, pTimeout);
}

EXPORT(int, _sceKernelWaitSignal, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    STUBBED("sceKernelWaitSignal");
    const auto thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    thread->update_status(ThreadStatus::wait);
    thread->signal.wait();
    thread->update_status(ThreadStatus::run);
    return SCE_KERNEL_OK;
}

EXPORT(int, _sceKernelWaitSignalCB, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    process_callbacks(emuenv.kernel, thread_id);
    return CALL_EXPORT(_sceKernelWaitSignal, unknown, delay, timeout);
}

int wait_thread_end(ThreadStatePtr &waiter, ThreadStatePtr &target, int *stat) {
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
    auto waiter = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    auto target = lock_and_find(thid, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!target) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    return wait_thread_end(waiter, target, stat);
}

EXPORT(int, _sceKernelWaitThreadEndCB, SceUID thid, int *stat, SceUInt *timeout) {
    auto waiter = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    auto target = lock_and_find(thid, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!target) {
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    }
    process_callbacks(emuenv.kernel, thid);
    return wait_thread_end(waiter, target, stat);
}

EXPORT(SceInt32, sceKernelCancelCallback, SceUID callbackId) {
    const CallbackPtr cb = lock_and_find(callbackId, emuenv.kernel.callbacks, emuenv.kernel.mutex);

    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);
    cb->cancel();

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelChangeActiveCpuMask) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelChangeThreadCpuAffinityMask, SceUID thid, SceInt32 affinity_mask) {
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thid ? thid : thread_id);

    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    const SceInt32 old_affinity = thread->affinity_mask;

    if (affinity_mask & ~SCE_KERNEL_CPU_MASK_USER_ALL)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK);

    thread->affinity_mask = affinity_mask;
    return old_affinity;
}

EXPORT(SceInt32, sceKernelChangeThreadPriority2, SceUID thid, SceInt32 priority) {
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

    return old_priority;
}

EXPORT(SceInt32, sceKernelChangeThreadPriority, SceUID thid, SceInt32 priority) {
    auto err = CALL_EXPORT(sceKernelChangeThreadPriority2, thid, priority);
    if (err < 0)
        return err;

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelChangeThreadVfpException) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelCheckCallback) {
    return process_callbacks(emuenv.kernel, thread_id);
}

EXPORT(int, sceKernelCheckWaitableStatus) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelClearEvent, SceUID event_id, SceUInt32 clear_pattern) {
    return simple_event_clear(emuenv.kernel, export_name, thread_id, event_id, clear_pattern);
}

EXPORT(SceInt32, sceKernelClearEventFlag, SceUID evfId, SceUInt32 bitPattern) {
    return eventflag_clear(emuenv.kernel, export_name, evfId, bitPattern);
}

EXPORT(int, sceKernelCloseCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseMutex_089) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseSimpleEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseTimer) {
    return STUBBED("References not implemented.");
}

EXPORT(SceUID, sceKernelCreateCallback, char *name, SceUInt32 attr, Ptr<SceKernelCallbackFunction> callbackFunc, Ptr<void> pCommon) {
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
    if (options->cpu_affinity_mask & ~SCE_KERNEL_CPU_MASK_USER_ALL) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_CPU_AFFINITY);
    }

    const ThreadStatePtr thread = emuenv.kernel.create_thread(emuenv.mem, name, entry.cast<void>(), init_priority, options->cpu_affinity_mask, options->stack_size, options->option.get(emuenv.mem));
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_ERROR);
    return thread->id;
}

int delay_thread(SceUInt delay_us) {
    if (delay_us == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    std::this_thread::sleep_for(std::chrono::microseconds(delay_us));

    return SCE_KERNEL_OK;
}

int delay_thread_cb(EmuEnvState &emuenv, SceUID thread_id, SceUInt delay_us) {
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
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThread200, SceUInt delay) {
    if (delay < 201)
        delay = 201;
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThreadCB, SceUInt delay) {
    return delay_thread_cb(emuenv, thread_id, delay);
}

EXPORT(int, sceKernelDelayThreadCB200, SceUInt delay) {
    if (delay < 201)
        delay = 201;
    return delay_thread_cb(emuenv, thread_id, delay);
}

EXPORT(int, sceKernelDeleteCallback) {
    // TODO
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteCond, SceUID condition_variable_id) {
    return condvar_delete(emuenv.kernel, export_name, thread_id, condition_variable_id, SyncWeight::Heavy);
}

EXPORT(int, sceKernelDeleteEventFlag, SceUID event_id) {
    return eventflag_delete(emuenv.kernel, export_name, thread_id, event_id);
}

EXPORT(int, sceKernelDeleteMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteMutex, SceUID mutexid) {
    return mutex_delete(emuenv.kernel, export_name, thread_id, mutexid, SyncWeight::Heavy);
}

EXPORT(SceInt32, sceKernelDeleteRWLock, SceUID lock_id) {
    return rwlock_delete(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id);
}

EXPORT(int, sceKernelDeleteSema, SceUID semaid) {
    return semaphore_delete(emuenv.kernel, export_name, thread_id, semaid);
}

EXPORT(int, sceKernelDeleteSimpleEvent, SceUID event_id) {
    return simple_event_delete(emuenv.kernel, export_name, thread_id, event_id);
}

EXPORT(int, sceKernelDeleteThread, SceUID thid) {
    const ThreadStatePtr thread = lock_and_find(thid, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread || thread->status != ThreadStatus::dormant) {
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    }
    emuenv.kernel.exit_delete_thread(thread);
    return 0;
}

EXPORT(int, sceKernelDeleteTimer, SceUID timer_handle) {
    emuenv.kernel.timers.erase(timer_handle);

    return 0;
}

EXPORT(int, sceKernelExitDeleteThread, int status) {
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    emuenv.kernel.exit_delete_thread(thread);

    return status;
}

EXPORT(SceInt32, sceKernelGetCallbackCount, SceUID callbackId) {
    const CallbackPtr cb = lock_and_find(callbackId, emuenv.kernel.callbacks, emuenv.kernel.mutex);

    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);

    return cb->get_num_notifications();
}

EXPORT(int, sceKernelGetMsgPipeCreatorId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessId) {
    STUBBED("pid: 0");
    return 0;
}

EXPORT(uint64_t, sceKernelGetSystemTimeWide) {
    return get_current_time();
}

EXPORT(SceInt32, sceKernelGetThreadCpuAffinityMask, SceUID thid) {
    return CALL_EXPORT(_sceKernelGetThreadCpuAffinityMask, thid);
}

EXPORT(int, sceKernelGetThreadStackFreeSize) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceKernelGetThreadTLSAddr, SceUID thid, int key) {
    return emuenv.kernel.get_thread_tls_addr(emuenv.mem, thid, key);
}

EXPORT(int, sceKernelGetThreadmgrUIDClass) {
    return UNIMPLEMENTED();
}

EXPORT(uint64_t, sceKernelGetTimerBaseWide, SceUID timer_handle) {
    const TimerPtr timer_info = lock_and_find(timer_handle, emuenv.kernel.timers, emuenv.kernel.mutex);

    if (!timer_info)
        return -1;

    return timer_info->time;
}

EXPORT(uint64_t, sceKernelGetTimerTimeWide, SceUID timer_handle) {
    const TimerPtr timer_info = lock_and_find(timer_handle, emuenv.kernel.timers, emuenv.kernel.mutex);

    if (!timer_info)
        return -1;

    return get_current_time() - timer_info->time;
}

EXPORT(SceInt32, sceKernelNotifyCallback, SceUID callbackId, SceInt32 notifyArg) {
    const CallbackPtr cb = lock_and_find(callbackId, emuenv.kernel.callbacks, emuenv.kernel.mutex);
    if (!cb)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID);

    cb->direct_notify(notifyArg);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelOpenCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenMsgPipe, const char *name) {
    return msgpipe_find(emuenv.kernel, export_name, name);
}

EXPORT(int, sceKernelOpenMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenMutex_089) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenSimpleEvent) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelOpenTimer, const char *name) {
    STUBBED("References not implemented.");

    SceUID timer_handle = -1;
    TimerPtr timer_info;

    emuenv.kernel.mutex.lock();
    for (const auto &timer : emuenv.kernel.timers) {
        if (timer.second->name == name) {
            timer_handle = timer.first;
            timer_info = timer.second;
            break;
        }
    }
    emuenv.kernel.mutex.unlock();

    assert(timer_info->openable);

    return timer_handle;
}

EXPORT(int, sceKernelPollSema, SceUID semaid, int32_t needCount) {
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
    return simple_event_setorpulse(emuenv.kernel, export_name, thread_id, event_id, set_pattern, user_data, false);
}

EXPORT(int, sceKernelRegisterCallbackToEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelResumeThreadForVM, SceUID threadId) {
    STUBBED("STUB");

    const ThreadStatePtr thread = lock_and_find(threadId, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    thread->resume();

    return 0;
}

EXPORT(int, sceKernelSendSignal, SceUID target_thread_id) {
    STUBBED("sceKernelSendSignal");
    const auto thread = lock_and_find(target_thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread->signal.send()) {
        return SCE_KERNEL_ERROR_ALREADY_SENT;
    }
    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, sceKernelSetEvent, SceUID event_id, SceUInt32 set_pattern, SceUInt64 user_data) {
    return simple_event_setorpulse(emuenv.kernel, export_name, thread_id, event_id, set_pattern, user_data, true);
}

EXPORT(SceInt32, sceKernelSetEventFlag, SceUID evfId, SceUInt32 bitPattern) {
    return eventflag_set(emuenv.kernel, export_name, thread_id, evfId, bitPattern);
}

EXPORT(int, sceKernelSetTimerTimeWide) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSignalCond, SceUID condid) {
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Any), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalCondAll, SceUID condid) {
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::All), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalCondTo, SceUID condid, SceUID thread_target) {
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Specific, thread_target), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalSema, SceUID semaid, int signal) {
    return semaphore_signal(emuenv.kernel, export_name, thread_id, semaid, signal);
}

EXPORT(int, sceKernelStartTimer, SceUID timer_handle) {
    const TimerPtr &timer_info = emuenv.kernel.timers[timer_handle];

    if (timer_info->is_started)
        return false;

    timer_info->is_started = true;
    timer_info->time = get_current_time();

    return true;
}

EXPORT(int, sceKernelStopTimer, SceUID timer_handle) {
    const TimerPtr timer_info = lock_and_find(timer_handle, emuenv.kernel.timers, emuenv.kernel.mutex);

    if (!timer_info->is_started)
        return false;

    timer_info->is_started = false;
    timer_info->time = get_current_time();

    return true;
}

EXPORT(int, sceKernelSuspendThreadForVM, SceUID threadId) {
    STUBBED("STUB");

    const ThreadStatePtr thread = lock_and_find(threadId, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    thread->suspend();

    return 0;
}

EXPORT(int, sceKernelTryLockMutex, SceUID mutexid, int lock_count) {
    return mutex_try_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, mutexid, lock_count, SyncWeight::Heavy);
}

EXPORT(int, sceKernelTryLockReadRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryLockWriteRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnlockMutex, SceUID mutexid, int unlock_count) {
    return mutex_unlock(emuenv.kernel, export_name, thread_id, mutexid, unlock_count, SyncWeight::Heavy);
}

EXPORT(int, sceKernelUnlockReadRWLock, SceUID lock_id) {
    return rwlock_unlock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, false);
}

EXPORT(int, sceKernelUnlockWriteRWLock, SceUID lock_id) {
    return rwlock_unlock(emuenv.kernel, emuenv.mem, export_name, thread_id, lock_id, true);
}

EXPORT(int, sceKernelUnregisterCallbackFromEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnregisterCallbackFromEventAll) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnregisterThreadEventHandler) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitThreadEndCB_089) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitThreadEnd_089) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(__sceKernelCreateLwMutex)
BRIDGE_IMPL(_sceKernelCancelEvent)
BRIDGE_IMPL(_sceKernelCancelEventFlag)
BRIDGE_IMPL(_sceKernelCancelEventWithSetPattern)
BRIDGE_IMPL(_sceKernelCancelMsgPipe)
BRIDGE_IMPL(_sceKernelCancelMutex)
BRIDGE_IMPL(_sceKernelCancelRWLock)
BRIDGE_IMPL(_sceKernelCancelSema)
BRIDGE_IMPL(_sceKernelCancelTimer)
BRIDGE_IMPL(_sceKernelCreateCond)
BRIDGE_IMPL(_sceKernelCreateEventFlag)
BRIDGE_IMPL(_sceKernelCreateLwCond)
BRIDGE_IMPL(_sceKernelCreateMsgPipeWithLR)
BRIDGE_IMPL(_sceKernelCreateMutex)
BRIDGE_IMPL(_sceKernelCreateRWLock)
BRIDGE_IMPL(_sceKernelCreateSema)
BRIDGE_IMPL(_sceKernelCreateSema_16XX)
BRIDGE_IMPL(_sceKernelCreateSimpleEvent)
BRIDGE_IMPL(_sceKernelCreateTimer)
BRIDGE_IMPL(_sceKernelDeleteLwCond)
BRIDGE_IMPL(_sceKernelDeleteLwMutex)
BRIDGE_IMPL(_sceKernelExitCallback)
BRIDGE_IMPL(_sceKernelGetCallbackInfo)
BRIDGE_IMPL(_sceKernelGetCondInfo)
BRIDGE_IMPL(_sceKernelGetEventFlagInfo)
BRIDGE_IMPL(_sceKernelGetEventInfo)
BRIDGE_IMPL(_sceKernelGetEventPattern)
BRIDGE_IMPL(_sceKernelGetLwCondInfo)
BRIDGE_IMPL(_sceKernelGetLwCondInfoById)
BRIDGE_IMPL(_sceKernelGetLwMutexInfoById)
BRIDGE_IMPL(_sceKernelGetMsgPipeInfo)
BRIDGE_IMPL(_sceKernelGetMutexInfo)
BRIDGE_IMPL(_sceKernelGetRWLockInfo)
BRIDGE_IMPL(_sceKernelGetSemaInfo)
BRIDGE_IMPL(_sceKernelGetSystemInfo)
BRIDGE_IMPL(_sceKernelGetSystemTime)
BRIDGE_IMPL(_sceKernelGetThreadContextForVM)
BRIDGE_IMPL(_sceKernelGetThreadCpuAffinityMask)
BRIDGE_IMPL(_sceKernelGetThreadEventInfo)
BRIDGE_IMPL(_sceKernelGetThreadExitStatus)
BRIDGE_IMPL(_sceKernelGetThreadInfo)
BRIDGE_IMPL(_sceKernelGetThreadRunStatus)
BRIDGE_IMPL(_sceKernelGetTimerBase)
BRIDGE_IMPL(_sceKernelGetTimerEventRemainingTime)
BRIDGE_IMPL(_sceKernelGetTimerInfo)
BRIDGE_IMPL(_sceKernelGetTimerTime)
BRIDGE_IMPL(_sceKernelLockLwMutex)
BRIDGE_IMPL(_sceKernelLockMutex)
BRIDGE_IMPL(_sceKernelLockMutexCB)
BRIDGE_IMPL(_sceKernelLockReadRWLock)
BRIDGE_IMPL(_sceKernelLockReadRWLockCB)
BRIDGE_IMPL(_sceKernelLockWriteRWLock)
BRIDGE_IMPL(_sceKernelLockWriteRWLockCB)
BRIDGE_IMPL(_sceKernelPMonThreadGetCounter)
BRIDGE_IMPL(_sceKernelPollEvent)
BRIDGE_IMPL(_sceKernelPollEventFlag)
BRIDGE_IMPL(_sceKernelPulseEventWithNotifyCallback)
BRIDGE_IMPL(_sceKernelReceiveMsgPipeVector)
BRIDGE_IMPL(_sceKernelReceiveMsgPipeVectorCB)
BRIDGE_IMPL(_sceKernelRegisterThreadEventHandler)
BRIDGE_IMPL(_sceKernelSendMsgPipeVector)
BRIDGE_IMPL(_sceKernelSendMsgPipeVectorCB)
BRIDGE_IMPL(_sceKernelSetEventWithNotifyCallback)
BRIDGE_IMPL(_sceKernelSetThreadContextForVM)
BRIDGE_IMPL(_sceKernelSetTimerEvent)
BRIDGE_IMPL(_sceKernelSetTimerTime)
BRIDGE_IMPL(_sceKernelSignalLwCond)
BRIDGE_IMPL(_sceKernelSignalLwCondAll)
BRIDGE_IMPL(_sceKernelSignalLwCondTo)
BRIDGE_IMPL(_sceKernelStartThread)
BRIDGE_IMPL(_sceKernelTryReceiveMsgPipeVector)
BRIDGE_IMPL(_sceKernelTrySendMsgPipeVector)
BRIDGE_IMPL(_sceKernelUnlockLwMutex)
BRIDGE_IMPL(_sceKernelWaitCond)
BRIDGE_IMPL(_sceKernelWaitCondCB)
BRIDGE_IMPL(_sceKernelWaitEvent)
BRIDGE_IMPL(_sceKernelWaitEventCB)
BRIDGE_IMPL(_sceKernelWaitEventFlag)
BRIDGE_IMPL(_sceKernelWaitEventFlagCB)
BRIDGE_IMPL(_sceKernelWaitException)
BRIDGE_IMPL(_sceKernelWaitExceptionCB)
BRIDGE_IMPL(_sceKernelWaitLwCond)
BRIDGE_IMPL(_sceKernelWaitLwCondCB)
BRIDGE_IMPL(_sceKernelWaitMultipleEvents)
BRIDGE_IMPL(_sceKernelWaitMultipleEventsCB)
BRIDGE_IMPL(_sceKernelWaitSema)
BRIDGE_IMPL(_sceKernelWaitSemaCB)
BRIDGE_IMPL(_sceKernelWaitSignal)
BRIDGE_IMPL(_sceKernelWaitSignalCB)
BRIDGE_IMPL(_sceKernelWaitThreadEnd)
BRIDGE_IMPL(_sceKernelWaitThreadEndCB)
BRIDGE_IMPL(sceKernelCancelCallback)
BRIDGE_IMPL(sceKernelChangeActiveCpuMask)
BRIDGE_IMPL(sceKernelChangeThreadCpuAffinityMask)
BRIDGE_IMPL(sceKernelChangeThreadPriority)
BRIDGE_IMPL(sceKernelChangeThreadPriority2)
BRIDGE_IMPL(sceKernelChangeThreadVfpException)
BRIDGE_IMPL(sceKernelCheckCallback)
BRIDGE_IMPL(sceKernelCheckWaitableStatus)
BRIDGE_IMPL(sceKernelClearEvent)
BRIDGE_IMPL(sceKernelClearEventFlag)
BRIDGE_IMPL(sceKernelCloseCond)
BRIDGE_IMPL(sceKernelCloseEventFlag)
BRIDGE_IMPL(sceKernelCloseMsgPipe)
BRIDGE_IMPL(sceKernelCloseMutex)
BRIDGE_IMPL(sceKernelCloseMutex_089)
BRIDGE_IMPL(sceKernelCloseRWLock)
BRIDGE_IMPL(sceKernelCloseSema)
BRIDGE_IMPL(sceKernelCloseSimpleEvent)
BRIDGE_IMPL(sceKernelCloseTimer)
BRIDGE_IMPL(sceKernelCreateCallback)
BRIDGE_IMPL(sceKernelCreateThreadForUser)
BRIDGE_IMPL(sceKernelDelayThread)
BRIDGE_IMPL(sceKernelDelayThread200)
BRIDGE_IMPL(sceKernelDelayThreadCB)
BRIDGE_IMPL(sceKernelDelayThreadCB200)
BRIDGE_IMPL(sceKernelDeleteCallback)
BRIDGE_IMPL(sceKernelDeleteCond)
BRIDGE_IMPL(sceKernelDeleteEventFlag)
BRIDGE_IMPL(sceKernelDeleteMsgPipe)
BRIDGE_IMPL(sceKernelDeleteMutex)
BRIDGE_IMPL(sceKernelDeleteRWLock)
BRIDGE_IMPL(sceKernelDeleteSema)
BRIDGE_IMPL(sceKernelDeleteSimpleEvent)
BRIDGE_IMPL(sceKernelDeleteThread)
BRIDGE_IMPL(sceKernelDeleteTimer)
BRIDGE_IMPL(sceKernelExitDeleteThread)
BRIDGE_IMPL(sceKernelGetCallbackCount)
BRIDGE_IMPL(sceKernelGetMsgPipeCreatorId)
BRIDGE_IMPL(sceKernelGetProcessId)
BRIDGE_IMPL(sceKernelGetSystemTimeWide)
BRIDGE_IMPL(sceKernelGetThreadCpuAffinityMask)
BRIDGE_IMPL(sceKernelGetThreadStackFreeSize)
BRIDGE_IMPL(sceKernelGetThreadTLSAddr)
BRIDGE_IMPL(sceKernelGetThreadmgrUIDClass)
BRIDGE_IMPL(sceKernelGetTimerBaseWide)
BRIDGE_IMPL(sceKernelGetTimerTimeWide)
BRIDGE_IMPL(sceKernelNotifyCallback)
BRIDGE_IMPL(sceKernelOpenCond)
BRIDGE_IMPL(sceKernelOpenEventFlag)
BRIDGE_IMPL(sceKernelOpenMsgPipe)
BRIDGE_IMPL(sceKernelOpenMutex)
BRIDGE_IMPL(sceKernelOpenMutex_089)
BRIDGE_IMPL(sceKernelOpenRWLock)
BRIDGE_IMPL(sceKernelOpenSema)
BRIDGE_IMPL(sceKernelOpenSimpleEvent)
BRIDGE_IMPL(sceKernelOpenTimer)
BRIDGE_IMPL(sceKernelPollSema)
BRIDGE_IMPL(sceKernelPulseEvent)
BRIDGE_IMPL(sceKernelRegisterCallbackToEvent)
BRIDGE_IMPL(sceKernelResumeThreadForVM)
BRIDGE_IMPL(sceKernelSendSignal)
BRIDGE_IMPL(sceKernelSetEvent)
BRIDGE_IMPL(sceKernelSetEventFlag)
BRIDGE_IMPL(sceKernelSetTimerTimeWide)
BRIDGE_IMPL(sceKernelSignalCond)
BRIDGE_IMPL(sceKernelSignalCondAll)
BRIDGE_IMPL(sceKernelSignalCondTo)
BRIDGE_IMPL(sceKernelSignalSema)
BRIDGE_IMPL(sceKernelStartTimer)
BRIDGE_IMPL(sceKernelStopTimer)
BRIDGE_IMPL(sceKernelSuspendThreadForVM)
BRIDGE_IMPL(sceKernelTryLockMutex)
BRIDGE_IMPL(sceKernelTryLockReadRWLock)
BRIDGE_IMPL(sceKernelTryLockWriteRWLock)
BRIDGE_IMPL(sceKernelUnlockMutex)
BRIDGE_IMPL(sceKernelUnlockReadRWLock)
BRIDGE_IMPL(sceKernelUnlockWriteRWLock)
BRIDGE_IMPL(sceKernelUnregisterCallbackFromEvent)
BRIDGE_IMPL(sceKernelUnregisterCallbackFromEventAll)
BRIDGE_IMPL(sceKernelUnregisterThreadEventHandler)
BRIDGE_IMPL(sceKernelWaitThreadEndCB_089)
BRIDGE_IMPL(sceKernelWaitThreadEnd_089)
