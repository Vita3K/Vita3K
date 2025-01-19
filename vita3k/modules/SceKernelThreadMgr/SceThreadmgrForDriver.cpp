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

#include "../SceLibKernel/SceLibKernel.h"
#include <module/module.h>

EXPORT(int, ksceKernelCancelCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCancelMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCancelMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelChangeCurrentThreadAttr) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelChangeThreadCpuAffinityMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelChangeThreadPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelChangeThreadSuspendStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelClearEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelClearEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateSimpleEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateThread) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteFastMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteThread) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelEnqueueWorkQueue) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetCallbackCount) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetMutexInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetProcessIdFromTLS) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetSystemTimeLow) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetTLSAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetThreadCpuAffinityMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetThreadCpuRegisters) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetThreadCurrentPriority) {
    return CALL_EXPORT(sceKernelGetThreadCurrentPriority);
}

EXPORT(int, ksceKernelGetThreadId) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetThreadIdList) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetThreadInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetThreadStackFreeSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetThreadTLSAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetThreadmgrUIDClass) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetTimerBaseWide) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetTimerTimeWide) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelInitializeFastMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelLockFastMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelLockMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelLockMutexCB_089) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelNotifyCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelPollEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelPollSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelPulseEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelPulseEventWithNotifyCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelReceiveMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelReceiveMsgPipeVectorCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelRegisterCallbackToEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelRegisterTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelRunWithStack) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSendMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSetEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSetEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSetPermission) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSetProcessId) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSetTimerTimeWide) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSignalCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSignalCondAll) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSignalCondTo) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSignalSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelStartThread) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelStartTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelStopTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelTryLockMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelTryLockReadRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelTryLockWriteRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelTryReceiveMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelTrySendMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelUnlockFastMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelUnlockMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelUnlockReadRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelUnlockWriteRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelUnregisterCallbackFromEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelUnregisterCallbackFromEventAll) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelWaitCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelWaitEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelWaitEventCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelWaitEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelWaitEventFlagCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelWaitSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelWaitThreadEnd) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelWaitThreadEndCB) {
    return UNIMPLEMENTED();
}
