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

#include "SceThreadmgr.h"

#include <host/functions.h>
#include <kernel/functions.h>
#include <kernel/thread/sync_primitives.h>
#include <util/lock_and_find.h>

#include <SDL_timer.h>
#include <psp2/kernel/error.h>

#include <chrono>
#include <thread>

EXPORT(int, __sceKernelCreateLwMutex) {
    return UNIMPLEMENTED();
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

EXPORT(int, _sceKernelCreateCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateLwCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateMsgPipeWithLR) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateSema_16XX) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateSimpleEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelDeleteLwCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelDeleteLwMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelExitCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetCallbackInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetCondInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetEventFlagInfo) {
    return UNIMPLEMENTED();
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

EXPORT(int, _sceKernelGetLwMutexInfoById) {
    return UNIMPLEMENTED();
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

EXPORT(int, _sceKernelGetSemaInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetSystemInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetSystemTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadContextForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadCpuAffinityMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadEventInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadExitStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelGetThreadInfo) {
    return UNIMPLEMENTED();
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

EXPORT(int, _sceKernelLockLwMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLockMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLockMutexCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLockReadRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLockReadRWLockCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLockWriteRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLockWriteRWLockCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPMonThreadGetCounter) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPollEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelPollEventFlag) {
    return UNIMPLEMENTED();
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

EXPORT(int, _sceKernelSetThreadContextForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetTimerEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSetTimerTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSignalLwCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSignalLwCondAll) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelSignalLwCondTo) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelStartThread) {
    return UNIMPLEMENTED();
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

EXPORT(int, _sceKernelWaitCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitCondCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitEventCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitEventFlagCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitException) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitExceptionCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitLwCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitLwCondCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitMultipleEvents) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitMultipleEventsCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitSemaCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitSignal) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitSignalCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitThreadEnd) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelWaitThreadEndCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelChangeActiveCpuMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelChangeThreadCpuAffinityMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelChangeThreadPriority, SceUID thid, int priority) {
    STUBBED("STUB");

    const ThreadStatePtr thread = lock_and_find(thid ? thid : thread_id, host.kernel.threads, host.kernel.mutex);
    const std::lock_guard<std::mutex> lock(thread->mutex);
    thread.get()->priority = priority;

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelChangeThreadPriority2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelChangeThreadVfpException) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCheckCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCheckWaitableStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelClearEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelClearEventFlag) {
    return UNIMPLEMENTED();
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
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCreateCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCreateThreadForUser) {
    return UNIMPLEMENTED();
}

int delay_thread(SceUInt delay_us) {
    if (delay_us == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    std::this_thread::sleep_for(std::chrono::microseconds(delay_us));

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDelayThread, SceUInt delay) {
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThread200, SceUInt delay) {
    STUBBED("untested");
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThreadCB, SceUInt delay) {
    STUBBED("no CB");
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThreadCB200, SceUInt delay) {
    STUBBED("no CB, untested");
    return delay_thread(delay);
}

EXPORT(int, sceKernelDeleteCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteCond, SceUID condition_variable_id) {
    return condvar_delete(host.kernel, export_name, thread_id, condition_variable_id, SyncWeight::Heavy);
}

EXPORT(int, sceKernelDeleteEventFlag, SceUID event_id) {
    return eventflag_delete(host.kernel, export_name, thread_id, event_id);
}

EXPORT(int, sceKernelDeleteMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteMutex, SceUID mutexid) {
    return mutex_delete(host.kernel, export_name, thread_id, mutexid, SyncWeight::Heavy);
}

EXPORT(int, sceKernelDeleteRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteSema, SceUID semaid) {
    return semaphore_delete(host.kernel, export_name, thread_id, semaid);
}

EXPORT(int, sceKernelDeleteSimpleEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteThread, SceUID thid) {
    const ThreadStatePtr thread = lock_and_find(thid, host.kernel.threads, host.kernel.mutex);

    // TODO: This causes a deadlock
    //const std::lock_guard<std::mutex> lock2(host.kernel.mutex);
    host.kernel.running_threads.erase(thid);
    host.kernel.waiting_threads.erase(thid);
    host.kernel.threads.erase(thid);
    return 0;
}

EXPORT(int, sceKernelDeleteTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelExitDeleteThread, int status) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    std::unique_lock<std::mutex> thread_lock(thread->mutex);

    thread->to_do = ThreadToDo::exit;
    stop(*thread->cpu);
    thread->something_to_do.notify_all();

    for (auto t : thread->waiting_threads) {
        const std::lock_guard<std::mutex> lock(t->mutex);
        assert(t->to_do == ThreadToDo::wait);
        t->to_do = ThreadToDo::run;
        t->something_to_do.notify_one();
    }

    thread->waiting_threads.clear();

    // need to unlock thread->mutex because thread destructor (delete_thread) will get called, and it locks that mutex
    thread_lock.unlock();

    // TODO: This causes a deadlock
    //const std::lock_guard<std::mutex> lock2(host.kernel.mutex);
    host.kernel.running_threads.erase(thread_id);
    host.kernel.waiting_threads.erase(thread_id);
    host.kernel.threads.erase(thread_id);

    return status;
}

EXPORT(int, sceKernelGetCallbackCount) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetMsgPipeCreatorId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessId) {
    STUBBED("pid: 0");
    return 0;
}

EXPORT(int, sceKernelGetSystemTimeWide) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadCpuAffinityMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadStackFreeSize) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceKernelGetThreadTLSAddr, SceUID thid, int key) {
    return get_thread_tls_addr(host.kernel, host.mem, thid, key);
}

EXPORT(int, sceKernelGetThreadmgrUIDClass) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerBaseWide) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerTimeWide) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelNotifyCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenMsgPipe) {
    return UNIMPLEMENTED();
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

EXPORT(int, sceKernelOpenTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPollSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPulseEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelRegisterCallbackToEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelResumeThreadForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSendSignal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetEventFlag, SceUID eventid, unsigned int flags) {
    return eventflag_set(host.kernel, export_name, thread_id, eventid, flags);
}

EXPORT(int, sceKernelSetTimerTimeWide) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSignalCond, SceUID condid) {
    return condvar_signal(host.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Any), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalCondAll, SceUID condid) {
    return condvar_signal(host.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::All), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalCondTo, SceUID condid, SceUID thread_target) {
    return condvar_signal(host.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Specific, thread_target), SyncWeight::Heavy);
}

EXPORT(int, sceKernelSignalSema, SceUID semaid, int signal) {
    return semaphore_signal(host.kernel, export_name, thread_id, semaid, signal);
}

EXPORT(int, sceKernelStartTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelStopTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSuspendThreadForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryLockMutex, SceUID mutexid, int lock_count) {
    return mutex_try_lock(host.kernel, export_name, thread_id, mutexid, lock_count, SyncWeight::Heavy);
}

EXPORT(int, sceKernelTryLockReadRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryLockWriteRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnlockMutex, SceUID mutexid, int unlock_count) {
    return mutex_unlock(host.kernel, export_name, thread_id, mutexid, unlock_count, SyncWeight::Heavy);
}

EXPORT(int, sceKernelUnlockReadRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnlockWriteRWLock) {
    return UNIMPLEMENTED();
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
