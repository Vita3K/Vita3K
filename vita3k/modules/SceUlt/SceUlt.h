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

#pragma once

#include <module/module.h>

BRIDGE_DECL(_sceUltConditionVariableCreate)
BRIDGE_DECL(_sceUltConditionVariableOptParamInitialize)
BRIDGE_DECL(_sceUltMutexCreate)
BRIDGE_DECL(_sceUltMutexOptParamInitialize)
BRIDGE_DECL(_sceUltQueueCreate)
BRIDGE_DECL(_sceUltQueueDataResourcePoolCreate)
BRIDGE_DECL(_sceUltQueueDataResourcePoolOptParamInitialize)
BRIDGE_DECL(_sceUltQueueOptParamInitialize)
BRIDGE_DECL(_sceUltReaderWriterLockCreate)
BRIDGE_DECL(_sceUltReaderWriterLockOptParamInitialize)
BRIDGE_DECL(_sceUltSemaphoreCreate)
BRIDGE_DECL(_sceUltSemaphoreOptParamInitialize)
BRIDGE_DECL(_sceUltUlthreadCreate)
BRIDGE_DECL(_sceUltUlthreadOptParamInitialize)
BRIDGE_DECL(_sceUltUlthreadRuntimeCreate)
BRIDGE_DECL(_sceUltUlthreadRuntimeOptParamInitialize)
BRIDGE_DECL(_sceUltWaitingQueueResourcePoolCreate)
BRIDGE_DECL(_sceUltWaitingQueueResourcePoolOptParamInitialize)
BRIDGE_DECL(sceUltConditionVariableDestroy)
BRIDGE_DECL(sceUltConditionVariableSignal)
BRIDGE_DECL(sceUltConditionVariableSignalAll)
BRIDGE_DECL(sceUltConditionVariableWait)
BRIDGE_DECL(sceUltGetConditionVariableInfo)
BRIDGE_DECL(sceUltGetMutexInfo)
BRIDGE_DECL(sceUltGetQueueDataResourcePoolInfo)
BRIDGE_DECL(sceUltGetQueueInfo)
BRIDGE_DECL(sceUltGetReaderWriterLockInfo)
BRIDGE_DECL(sceUltGetSemaphoreInfo)
BRIDGE_DECL(sceUltGetUlthreadInfo)
BRIDGE_DECL(sceUltGetUlthreadRuntimeInfo)
BRIDGE_DECL(sceUltGetWaitingQueueResourcePoolInfo)
BRIDGE_DECL(sceUltMutexDestroy)
BRIDGE_DECL(sceUltMutexLock)
BRIDGE_DECL(sceUltMutexTryLock)
BRIDGE_DECL(sceUltMutexUnlock)
BRIDGE_DECL(sceUltQueueDataResourcePoolDestroy)
BRIDGE_DECL(sceUltQueueDataResourcePoolGetWorkAreaSize)
BRIDGE_DECL(sceUltQueueDestroy)
BRIDGE_DECL(sceUltQueuePop)
BRIDGE_DECL(sceUltQueuePush)
BRIDGE_DECL(sceUltQueueTryPop)
BRIDGE_DECL(sceUltQueueTryPush)
BRIDGE_DECL(sceUltReaderWriterLockDestroy)
BRIDGE_DECL(sceUltReaderWriterLockLockRead)
BRIDGE_DECL(sceUltReaderWriterLockLockWrite)
BRIDGE_DECL(sceUltReaderWriterLockTryLockRead)
BRIDGE_DECL(sceUltReaderWriterLockTryLockWrite)
BRIDGE_DECL(sceUltReaderWriterLockUnlockRead)
BRIDGE_DECL(sceUltReaderWriterLockUnlockWrite)
BRIDGE_DECL(sceUltSemaphoreAcquire)
BRIDGE_DECL(sceUltSemaphoreDestroy)
BRIDGE_DECL(sceUltSemaphoreRelease)
BRIDGE_DECL(sceUltSemaphoreTryAcquire)
BRIDGE_DECL(sceUltUlthreadExit)
BRIDGE_DECL(sceUltUlthreadGetSelf)
BRIDGE_DECL(sceUltUlthreadJoin)
BRIDGE_DECL(sceUltUlthreadRuntimeDestroy)
BRIDGE_DECL(sceUltUlthreadRuntimeGetWorkAreaSize)
BRIDGE_DECL(sceUltUlthreadTryJoin)
BRIDGE_DECL(sceUltUlthreadYield)
BRIDGE_DECL(sceUltWaitingQueueResourcePoolDestroy)
BRIDGE_DECL(sceUltWaitingQueueResourcePoolGetWorkAreaSize)
