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
    return unimplemented("sceKernelCancelCallback");
}

EXPORT(int, sceKernelChangeActiveCpuMask) {
    return unimplemented("sceKernelChangeActiveCpuMask");
}

EXPORT(int, sceKernelChangeThreadCpuAffinityMask) {
    return unimplemented("sceKernelChangeThreadCpuAffinityMask");
}

EXPORT(int, sceKernelChangeThreadPriority) {
    return unimplemented("sceKernelChangeThreadPriority");
}

EXPORT(int, sceKernelChangeThreadPriority2) {
    return unimplemented("sceKernelChangeThreadPriority2");
}

EXPORT(int, sceKernelChangeThreadVfpException) {
    return unimplemented("sceKernelChangeThreadVfpException");
}

EXPORT(int, sceKernelCheckCallback) {
    return unimplemented("sceKernelCheckCallback");
}

EXPORT(int, sceKernelCheckWaitableStatus) {
    return unimplemented("sceKernelCheckWaitableStatus");
}

EXPORT(int, sceKernelClearEvent) {
    return unimplemented("sceKernelClearEvent");
}

EXPORT(int, sceKernelClearEventFlag) {
    return unimplemented("sceKernelClearEventFlag");
}

EXPORT(int, sceKernelCloseCond) {
    return unimplemented("sceKernelCloseCond");
}

EXPORT(int, sceKernelCloseEventFlag) {
    return unimplemented("sceKernelCloseEventFlag");
}

EXPORT(int, sceKernelCloseMsgPipe) {
    return unimplemented("sceKernelCloseMsgPipe");
}

EXPORT(int, sceKernelCloseMutex) {
    return unimplemented("sceKernelCloseMutex");
}

EXPORT(int, sceKernelCloseRWLock) {
    return unimplemented("sceKernelCloseRWLock");
}

EXPORT(int, sceKernelCloseSema) {
    return unimplemented("sceKernelCloseSema");
}

EXPORT(int, sceKernelCloseSimpleEvent) {
    return unimplemented("sceKernelCloseSimpleEvent");
}

EXPORT(int, sceKernelCloseTimer) {
    return unimplemented("sceKernelCloseTimer");
}

EXPORT(int, sceKernelCreateCallback) {
    return unimplemented("sceKernelCreateCallback");
}

EXPORT(int, sceKernelCreateThreadForUser) {
    return unimplemented("sceKernelCreateThreadForUser");
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
    return unimplemented("sceKernelDelayThread200");
}

EXPORT(int, sceKernelDelayThreadCB) {
    return unimplemented("sceKernelDelayThreadCB");
}

EXPORT(int, sceKernelDelayThreadCB200) {
    return unimplemented("sceKernelDelayThreadCB200");
}

EXPORT(int, sceKernelDeleteCallback) {
    return unimplemented("sceKernelDeleteCallback");
}

EXPORT(int, sceKernelDeleteCond) {
    return unimplemented("sceKernelDeleteCond");
}

EXPORT(int, sceKernelDeleteEventFlag) {
    return unimplemented("sceKernelDeleteEventFlag");
}

EXPORT(int, sceKernelDeleteMsgPipe) {
    return unimplemented("sceKernelDeleteMsgPipe");
}

EXPORT(int, sceKernelDeleteMutex) {
    return unimplemented("sceKernelDeleteMutex");
}

EXPORT(int, sceKernelDeleteRWLock) {
    return unimplemented("sceKernelDeleteRWLock");
}

EXPORT(int, sceKernelDeleteSema) {
    return unimplemented("sceKernelDeleteSema");
}

EXPORT(int, sceKernelDeleteSimpleEvent) {
    return unimplemented("sceKernelDeleteSimpleEvent");
}

EXPORT(int, sceKernelDeleteThread) {
    return unimplemented("sceKernelDeleteThread");
}

EXPORT(int, sceKernelDeleteTimer) {
    return unimplemented("sceKernelDeleteTimer");
}

EXPORT(int, sceKernelExitDeleteThread) {
    return unimplemented("sceKernelExitDeleteThread");
}

EXPORT(int, sceKernelGetCallbackCount) {
    return unimplemented("sceKernelGetCallbackCount");
}

EXPORT(int, sceKernelGetMsgPipeCreatorId) {
    return unimplemented("sceKernelGetMsgPipeCreatorId");
}

EXPORT(int, sceKernelGetProcessId) {
    return unimplemented("sceKernelGetProcessId");
}

EXPORT(int, sceKernelGetSystemTimeWide) {
    return unimplemented("sceKernelGetSystemTimeWide");
}

EXPORT(int, sceKernelGetThreadStackFreeSize) {
    return unimplemented("sceKernelGetThreadStackFreeSize");
}

EXPORT(Ptr<void>, sceKernelGetThreadTLSAddr, SceUID thid, int key) {
    return get_thread_tls_addr(host.kernel, host.mem, thid, key);
}

EXPORT(int, sceKernelGetThreadmgrUIDClass) {
    return unimplemented("sceKernelGetThreadmgrUIDClass");
}

EXPORT(int, sceKernelGetTimerBaseWide) {
    return unimplemented("sceKernelGetTimerBaseWide");
}

EXPORT(int, sceKernelGetTimerTimeWide) {
    return unimplemented("sceKernelGetTimerTimeWide");
}

EXPORT(int, sceKernelNotifyCallback) {
    return unimplemented("sceKernelNotifyCallback");
}

EXPORT(int, sceKernelOpenCond) {
    return unimplemented("sceKernelOpenCond");
}

EXPORT(int, sceKernelOpenEventFlag) {
    return unimplemented("sceKernelOpenEventFlag");
}

EXPORT(int, sceKernelOpenMsgPipe) {
    return unimplemented("sceKernelOpenMsgPipe");
}

EXPORT(int, sceKernelOpenMutex) {
    return unimplemented("sceKernelOpenMutex");
}

EXPORT(int, sceKernelOpenRWLock) {
    return unimplemented("sceKernelOpenRWLock");
}

EXPORT(int, sceKernelOpenSema) {
    return unimplemented("sceKernelOpenSema");
}

EXPORT(int, sceKernelOpenSimpleEvent) {
    return unimplemented("sceKernelOpenSimpleEvent");
}

EXPORT(int, sceKernelOpenTimer) {
    return unimplemented("sceKernelOpenTimer");
}

EXPORT(int, sceKernelPollSema) {
    return unimplemented("sceKernelPollSema");
}

EXPORT(int, sceKernelPulseEvent) {
    return unimplemented("sceKernelPulseEvent");
}

EXPORT(int, sceKernelRegisterCallbackToEvent) {
    return unimplemented("sceKernelRegisterCallbackToEvent");
}

EXPORT(int, sceKernelSendSignal) {
    return unimplemented("sceKernelSendSignal");
}

EXPORT(int, sceKernelSetEvent) {
    return unimplemented("sceKernelSetEvent");
}

EXPORT(int, sceKernelSetEventFlag) {
    return unimplemented("sceKernelSetEventFlag");
}

EXPORT(int, sceKernelSetTimerTimeWide) {
    return unimplemented("sceKernelSetTimerTimeWide");
}

EXPORT(int, sceKernelSignalCond) {
    return unimplemented("sceKernelSignalCond");
}

EXPORT(int, sceKernelSignalCondAll) {
    return unimplemented("sceKernelSignalCondAll");
}

EXPORT(int, sceKernelSignalCondTo) {
    return unimplemented("sceKernelSignalCondTo");
}

EXPORT(int, sceKernelSignalSema) {
    return unimplemented("sceKernelSignalSema");
}

EXPORT(int, sceKernelStartTimer) {
    return unimplemented("sceKernelStartTimer");
}

EXPORT(int, sceKernelStopTimer) {
    return unimplemented("sceKernelStopTimer");
}

EXPORT(int, sceKernelTryLockMutex) {
    return unimplemented("sceKernelTryLockMutex");
}

EXPORT(int, sceKernelTryLockReadRWLock) {
    return unimplemented("sceKernelTryLockReadRWLock");
}

EXPORT(int, sceKernelTryLockWriteRWLock) {
    return unimplemented("sceKernelTryLockWriteRWLock");
}

EXPORT(int, sceKernelUnlockMutex) {
    return unimplemented("sceKernelUnlockMutex");
}

EXPORT(int, sceKernelUnlockReadRWLock) {
    return unimplemented("sceKernelUnlockReadRWLock");
}

EXPORT(int, sceKernelUnlockWriteRWLock) {
    return unimplemented("sceKernelUnlockWriteRWLock");
}

EXPORT(int, sceKernelUnregisterCallbackFromEvent) {
    return unimplemented("sceKernelUnregisterCallbackFromEvent");
}

EXPORT(int, sceKernelUnregisterCallbackFromEventAll) {
    return unimplemented("sceKernelUnregisterCallbackFromEventAll");
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
