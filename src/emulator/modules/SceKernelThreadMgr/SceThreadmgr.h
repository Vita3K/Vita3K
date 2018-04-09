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
BRIDGE_DECL(sceKernelOpenRWLock)
BRIDGE_DECL(sceKernelOpenSema)
BRIDGE_DECL(sceKernelOpenSimpleEvent)
BRIDGE_DECL(sceKernelOpenTimer)
BRIDGE_DECL(sceKernelPollSema)
BRIDGE_DECL(sceKernelPulseEvent)
BRIDGE_DECL(sceKernelRegisterCallbackToEvent)
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
BRIDGE_DECL(sceKernelTryLockMutex)
BRIDGE_DECL(sceKernelTryLockReadRWLock)
BRIDGE_DECL(sceKernelTryLockWriteRWLock)
BRIDGE_DECL(sceKernelUnlockMutex)
BRIDGE_DECL(sceKernelUnlockReadRWLock)
BRIDGE_DECL(sceKernelUnlockWriteRWLock)
BRIDGE_DECL(sceKernelUnregisterCallbackFromEvent)
BRIDGE_DECL(sceKernelUnregisterCallbackFromEventAll)
BRIDGE_DECL(sceKernelUnregisterThreadEventHandler)
