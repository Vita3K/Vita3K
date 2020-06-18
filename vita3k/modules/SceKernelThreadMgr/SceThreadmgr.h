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

#pragma once

#include <module/module.h>

// TODO figure out more about this struct
struct SceKernelWaitSignalResult {
    Address tls_address;
    uint32_t dret;
};

// TODO figure out more about this struct
struct SceKernelWaitSignalParams {
    uint32_t reserved[2];
    Ptr<SceKernelWaitSignalResult> result_ptr;
};

EXPORT(int, __sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, Ptr<SceKernelCreateLwMutex_opt> opt);
EXPORT(int, _sceKernelDeleteLwMutex, Ptr<SceKernelLwMutexWork> workarea);
EXPORT(int, _sceKernelLockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout);
EXPORT(int, _sceKernelCreateLwCond, Ptr<SceKernelLwCondWork> workarea, const char *name, SceUInt attr, Ptr<SceKernelCreateLwCond_opt> opt);
EXPORT(int, sceKernelCreateThreadForUser, const char *name, SceKernelThreadEntry entry, int init_priority, SceKernelCreateThread_opt *options);
EXPORT(int, _sceKernelStartThread, SceUID thid, SceSize arglen, Ptr<void> argp);
EXPORT(int, _sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout);
EXPORT(int, _sceKernelCreateEventFlag, const char *name, unsigned int attr, unsigned int flags, SceKernelEventFlagOptParam *opt);
EXPORT(int, _sceKernelCreateSema, const char *name, SceUInt attr, int initVal, Ptr<SceKernelCreateSema_opt> opt);
EXPORT(int, _sceKernelWaitSema, SceUID semaid, int signal, SceUInt *timeout);
EXPORT(int, _sceKernelPollEventFlag, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits);
EXPORT(int, _sceKernelWaitThreadEnd, SceUID thid, int *stat, SceUInt *timeout);
EXPORT(int, _sceKernelWaitThreadEndCB, SceUID thid, int *stat, SceUInt *timeout);
EXPORT(int, _sceKernelWaitSignal, uint32_t unknown, uint32_t delay, uint32_t timeout, SceKernelWaitSignalParams *params);

BRIDGE_DECL(__sceKernelCreateLwMutex)
BRIDGE_DECL(_sceKernelCancelEvent)
BRIDGE_DECL(_sceKernelCancelEventFlag)
BRIDGE_DECL(_sceKernelCancelEventWithSetPattern)
BRIDGE_DECL(_sceKernelCancelMsgPipe)
BRIDGE_DECL(_sceKernelCancelMutex)
BRIDGE_DECL(_sceKernelCancelRWLock)
BRIDGE_DECL(_sceKernelCancelSema)
BRIDGE_DECL(_sceKernelCancelTimer)
BRIDGE_DECL(_sceKernelCreateCond)
BRIDGE_DECL(_sceKernelCreateEventFlag)
BRIDGE_DECL(_sceKernelCreateLwCond)
BRIDGE_DECL(_sceKernelCreateMsgPipeWithLR)
BRIDGE_DECL(_sceKernelCreateMutex)
BRIDGE_DECL(_sceKernelCreateRWLock)
BRIDGE_DECL(_sceKernelCreateSema)
BRIDGE_DECL(_sceKernelCreateSema_16XX)
BRIDGE_DECL(_sceKernelCreateSimpleEvent)
BRIDGE_DECL(_sceKernelCreateTimer)
BRIDGE_DECL(_sceKernelDeleteLwCond)
BRIDGE_DECL(_sceKernelDeleteLwMutex)
BRIDGE_DECL(_sceKernelExitCallback)
BRIDGE_DECL(_sceKernelGetCallbackInfo)
BRIDGE_DECL(_sceKernelGetCondInfo)
BRIDGE_DECL(_sceKernelGetEventFlagInfo)
BRIDGE_DECL(_sceKernelGetEventInfo)
BRIDGE_DECL(_sceKernelGetEventPattern)
BRIDGE_DECL(_sceKernelGetLwCondInfo)
BRIDGE_DECL(_sceKernelGetLwCondInfoById)
BRIDGE_DECL(_sceKernelGetLwMutexInfoById)
BRIDGE_DECL(_sceKernelGetMsgPipeInfo)
BRIDGE_DECL(_sceKernelGetMutexInfo)
BRIDGE_DECL(_sceKernelGetRWLockInfo)
BRIDGE_DECL(_sceKernelGetSemaInfo)
BRIDGE_DECL(_sceKernelGetSystemInfo)
BRIDGE_DECL(_sceKernelGetSystemTime)
BRIDGE_DECL(_sceKernelGetThreadContextForVM)
BRIDGE_DECL(_sceKernelGetThreadCpuAffinityMask)
BRIDGE_DECL(_sceKernelGetThreadEventInfo)
BRIDGE_DECL(_sceKernelGetThreadExitStatus)
BRIDGE_DECL(_sceKernelGetThreadInfo)
BRIDGE_DECL(_sceKernelGetThreadRunStatus)
BRIDGE_DECL(_sceKernelGetTimerBase)
BRIDGE_DECL(_sceKernelGetTimerEventRemainingTime)
BRIDGE_DECL(_sceKernelGetTimerInfo)
BRIDGE_DECL(_sceKernelGetTimerTime)
BRIDGE_DECL(_sceKernelLockLwMutex)
BRIDGE_DECL(_sceKernelLockMutex)
BRIDGE_DECL(_sceKernelLockMutexCB)
BRIDGE_DECL(_sceKernelLockReadRWLock)
BRIDGE_DECL(_sceKernelLockReadRWLockCB)
BRIDGE_DECL(_sceKernelLockWriteRWLock)
BRIDGE_DECL(_sceKernelLockWriteRWLockCB)
BRIDGE_DECL(_sceKernelPMonThreadGetCounter)
BRIDGE_DECL(_sceKernelPollEvent)
BRIDGE_DECL(_sceKernelPollEventFlag)
BRIDGE_DECL(_sceKernelPulseEventWithNotifyCallback)
BRIDGE_DECL(_sceKernelReceiveMsgPipeVector)
BRIDGE_DECL(_sceKernelReceiveMsgPipeVectorCB)
BRIDGE_DECL(_sceKernelRegisterThreadEventHandler)
BRIDGE_DECL(_sceKernelSendMsgPipeVector)
BRIDGE_DECL(_sceKernelSendMsgPipeVectorCB)
BRIDGE_DECL(_sceKernelSetEventWithNotifyCallback)
BRIDGE_DECL(_sceKernelSetThreadContextForVM)
BRIDGE_DECL(_sceKernelSetTimerEvent)
BRIDGE_DECL(_sceKernelSetTimerTime)
BRIDGE_DECL(_sceKernelSignalLwCond)
BRIDGE_DECL(_sceKernelSignalLwCondAll)
BRIDGE_DECL(_sceKernelSignalLwCondTo)
BRIDGE_DECL(_sceKernelStartThread)
BRIDGE_DECL(_sceKernelTryReceiveMsgPipeVector)
BRIDGE_DECL(_sceKernelTrySendMsgPipeVector)
BRIDGE_DECL(_sceKernelUnlockLwMutex)
BRIDGE_DECL(_sceKernelWaitCond)
BRIDGE_DECL(_sceKernelWaitCondCB)
BRIDGE_DECL(_sceKernelWaitEvent)
BRIDGE_DECL(_sceKernelWaitEventCB)
BRIDGE_DECL(_sceKernelWaitEventFlag)
BRIDGE_DECL(_sceKernelWaitEventFlagCB)
BRIDGE_DECL(_sceKernelWaitException)
BRIDGE_DECL(_sceKernelWaitExceptionCB)
BRIDGE_DECL(_sceKernelWaitLwCond)
BRIDGE_DECL(_sceKernelWaitLwCondCB)
BRIDGE_DECL(_sceKernelWaitMultipleEvents)
BRIDGE_DECL(_sceKernelWaitMultipleEventsCB)
BRIDGE_DECL(_sceKernelWaitSema)
BRIDGE_DECL(_sceKernelWaitSemaCB)
BRIDGE_DECL(_sceKernelWaitSignal)
BRIDGE_DECL(_sceKernelWaitSignalCB)
BRIDGE_DECL(_sceKernelWaitThreadEnd)
BRIDGE_DECL(_sceKernelWaitThreadEndCB)
BRIDGE_DECL(sceKernelCancelCallback)
BRIDGE_DECL(sceKernelChangeActiveCpuMask)
BRIDGE_DECL(sceKernelChangeThreadCpuAffinityMask)
BRIDGE_DECL(sceKernelChangeThreadPriority)
BRIDGE_DECL(sceKernelChangeThreadPriority2)
BRIDGE_DECL(sceKernelChangeThreadVfpException)
BRIDGE_DECL(sceKernelCheckCallback)
BRIDGE_DECL(sceKernelCheckWaitableStatus)
BRIDGE_DECL(sceKernelClearEvent)
BRIDGE_DECL(sceKernelClearEventFlag)
BRIDGE_DECL(sceKernelCloseCond)
BRIDGE_DECL(sceKernelCloseEventFlag)
BRIDGE_DECL(sceKernelCloseMsgPipe)
BRIDGE_DECL(sceKernelCloseMutex)
BRIDGE_DECL(sceKernelCloseMutex_089)
BRIDGE_DECL(sceKernelCloseRWLock)
BRIDGE_DECL(sceKernelCloseSema)
BRIDGE_DECL(sceKernelCloseSimpleEvent)
BRIDGE_DECL(sceKernelCloseTimer)
BRIDGE_DECL(sceKernelCreateCallback)
BRIDGE_DECL(sceKernelCreateThreadForUser)
BRIDGE_DECL(sceKernelDelayThread)
BRIDGE_DECL(sceKernelDelayThread200)
BRIDGE_DECL(sceKernelDelayThreadCB)
BRIDGE_DECL(sceKernelDelayThreadCB200)
BRIDGE_DECL(sceKernelDeleteCallback)
BRIDGE_DECL(sceKernelDeleteCond)
BRIDGE_DECL(sceKernelDeleteEventFlag)
BRIDGE_DECL(sceKernelDeleteMsgPipe)
BRIDGE_DECL(sceKernelDeleteMutex)
BRIDGE_DECL(sceKernelDeleteRWLock)
BRIDGE_DECL(sceKernelDeleteSema)
BRIDGE_DECL(sceKernelDeleteSimpleEvent)
BRIDGE_DECL(sceKernelDeleteThread)
BRIDGE_DECL(sceKernelDeleteTimer)
BRIDGE_DECL(sceKernelExitDeleteThread)
BRIDGE_DECL(sceKernelGetCallbackCount)
BRIDGE_DECL(sceKernelGetMsgPipeCreatorId)
BRIDGE_DECL(sceKernelGetProcessId)
BRIDGE_DECL(sceKernelGetSystemTimeWide)
BRIDGE_DECL(sceKernelGetThreadCpuAffinityMask)
BRIDGE_DECL(sceKernelGetThreadStackFreeSize)
BRIDGE_DECL(sceKernelGetThreadTLSAddr)
BRIDGE_DECL(sceKernelGetThreadmgrUIDClass)
BRIDGE_DECL(sceKernelGetTimerBaseWide)
BRIDGE_DECL(sceKernelGetTimerTimeWide)
BRIDGE_DECL(sceKernelNotifyCallback)
BRIDGE_DECL(sceKernelOpenCond)
BRIDGE_DECL(sceKernelOpenEventFlag)
BRIDGE_DECL(sceKernelOpenMsgPipe)
BRIDGE_DECL(sceKernelOpenMutex)
BRIDGE_DECL(sceKernelOpenMutex_089)
BRIDGE_DECL(sceKernelOpenRWLock)
BRIDGE_DECL(sceKernelOpenSema)
BRIDGE_DECL(sceKernelOpenSimpleEvent)
BRIDGE_DECL(sceKernelOpenTimer)
BRIDGE_DECL(sceKernelPollSema)
BRIDGE_DECL(sceKernelPulseEvent)
BRIDGE_DECL(sceKernelRegisterCallbackToEvent)
BRIDGE_DECL(sceKernelResumeThreadForVM)
BRIDGE_DECL(sceKernelSendSignal)
BRIDGE_DECL(sceKernelSetEvent)
BRIDGE_DECL(sceKernelSetEventFlag)
BRIDGE_DECL(sceKernelSetTimerTimeWide)
BRIDGE_DECL(sceKernelSignalCond)
BRIDGE_DECL(sceKernelSignalCondAll)
BRIDGE_DECL(sceKernelSignalCondTo)
BRIDGE_DECL(sceKernelSignalSema)
BRIDGE_DECL(sceKernelStartTimer)
BRIDGE_DECL(sceKernelStopTimer)
BRIDGE_DECL(sceKernelSuspendThreadForVM)
BRIDGE_DECL(sceKernelTryLockMutex)
BRIDGE_DECL(sceKernelTryLockReadRWLock)
BRIDGE_DECL(sceKernelTryLockWriteRWLock)
BRIDGE_DECL(sceKernelUnlockMutex)
BRIDGE_DECL(sceKernelUnlockReadRWLock)
BRIDGE_DECL(sceKernelUnlockWriteRWLock)
BRIDGE_DECL(sceKernelUnregisterCallbackFromEvent)
BRIDGE_DECL(sceKernelUnregisterCallbackFromEventAll)
BRIDGE_DECL(sceKernelUnregisterThreadEventHandler)
BRIDGE_DECL(sceKernelWaitThreadEndCB_089)
BRIDGE_DECL(sceKernelWaitThreadEnd_089)
