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

#include <SDL_timer.h>
#include <psp2/kernel/error.h>

EXPORT(int, sceKernelCancelCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelChangeActiveCpuMask) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelChangeThreadCpuAffinityMask) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelChangeThreadPriority) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelChangeThreadPriority2) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelChangeThreadVfpException) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCheckCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCheckWaitableStatus) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelClearEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelClearEventFlag) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCloseCond) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCloseEventFlag) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCloseMsgPipe) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCloseMutex) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCloseRWLock) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCloseSema) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCloseSimpleEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCloseTimer) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCreateCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelCreateThreadForUser) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDelayThread, SceUInt delay) {
#ifdef _WIN32
    Sleep(delay / 1000);
#else
    usleep(delay);
#endif
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDelayThread200) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDelayThreadCB) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDelayThreadCB200) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteCond) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteEventFlag) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteMsgPipe) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteMutex, SceInt32 mutexid) {
    return delete_mutex(host, thread_id, host.kernel.mutexes, mutexid);
}

EXPORT(int, sceKernelDeleteRWLock) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteSema) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteSimpleEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteThread) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelDeleteTimer) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelExitDeleteThread) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelGetCallbackCount) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelGetMsgPipeCreatorId) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelGetProcessId) {
    unimplemented(export_name);
    return 0;
}

EXPORT(int, sceKernelGetSystemTimeWide) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelGetThreadStackFreeSize) {
    return unimplemented(export_name);
}

EXPORT(Ptr<void>, sceKernelGetThreadTLSAddr, SceUID thid, int key) {
    return get_thread_tls_addr(host.kernel, host.mem, thid, key);
}

EXPORT(int, sceKernelGetThreadmgrUIDClass) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelGetTimerBaseWide) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelGetTimerTimeWide) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelNotifyCallback) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelOpenCond) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelOpenEventFlag) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelOpenMsgPipe) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelOpenMutex) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelOpenRWLock) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelOpenSema) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelOpenSimpleEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelOpenTimer) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelPollSema) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelPulseEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelRegisterCallbackToEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelSendSignal) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelSetEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelSetEventFlag) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelSetTimerTimeWide) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelSignalCond) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelSignalCondAll) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelSignalCondTo) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelSignalSema, SceUID semaid, int signal) {
    // TODO Don't lock twice.
    const SemaphorePtr semaphore = lock_and_find(semaid, host.kernel.semaphores, host.kernel.mutex);
    if (!semaphore) {
        return error("sceKernelSignalSema", SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    const std::unique_lock<std::mutex> lock(semaphore->mutex);

    semaphore->val += signal;
    if (semaphore->val > semaphore->max) {
        semaphore->val = semaphore->max;
    }

    while (semaphore->val > 0 && semaphore->waiting_threads.size() > 0) {
        const ThreadStatePtr thread = semaphore->waiting_threads.back();
        assert(thread->to_do == ThreadToDo::wait);
        thread->to_do = ThreadToDo::run;
        semaphore->waiting_threads.pop_back();
        semaphore->val--;
        thread->something_to_do.notify_one();
    }

    return 0;
}

EXPORT(int, sceKernelStartTimer) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelStopTimer) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelTryLockMutex) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelTryLockReadRWLock) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelTryLockWriteRWLock) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelUnlockMutex, SceUID mutexid, int unlock_count) {
    return unlock_mutex(host, thread_id, host.kernel.mutexes, mutexid, unlock_count);
}

EXPORT(int, sceKernelUnlockReadRWLock) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelUnlockWriteRWLock) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelUnregisterCallbackFromEvent) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelUnregisterCallbackFromEventAll) {
    return unimplemented(export_name);
}

EXPORT(int, sceKernelUnregisterThreadEventHandler) {
    return unimplemented(export_name);
}

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
BRIDGE_IMPL(sceKernelOpenRWLock)
BRIDGE_IMPL(sceKernelOpenSema)
BRIDGE_IMPL(sceKernelOpenSimpleEvent)
BRIDGE_IMPL(sceKernelOpenTimer)
BRIDGE_IMPL(sceKernelPollSema)
BRIDGE_IMPL(sceKernelPulseEvent)
BRIDGE_IMPL(sceKernelRegisterCallbackToEvent)
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
BRIDGE_IMPL(sceKernelTryLockMutex)
BRIDGE_IMPL(sceKernelTryLockReadRWLock)
BRIDGE_IMPL(sceKernelTryLockWriteRWLock)
BRIDGE_IMPL(sceKernelUnlockMutex)
BRIDGE_IMPL(sceKernelUnlockReadRWLock)
BRIDGE_IMPL(sceKernelUnlockWriteRWLock)
BRIDGE_IMPL(sceKernelUnregisterCallbackFromEvent)
BRIDGE_IMPL(sceKernelUnregisterCallbackFromEventAll)
BRIDGE_IMPL(sceKernelUnregisterThreadEventHandler)
