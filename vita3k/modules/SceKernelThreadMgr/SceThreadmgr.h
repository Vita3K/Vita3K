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

#pragma once

#include <module/module.h>

#include <kernel/types.h>

struct sceKernelRegisterThreadEventHandlerOpt {
    Ptr<const void> handler;
    Address common;
    int reserved0;
    int reserved1;
};

DECL_EXPORT(SceUID, _sceKernelCreateCond, const char *pName, SceUInt32 attr, SceUID mutexId, const SceKernelCondOptParam *pOptParam);
DECL_EXPORT(SceInt32, _sceKernelGetCondInfo, SceUID condId, Ptr<SceKernelCondInfo> pInfo);
DECL_EXPORT(SceUID, _sceKernelCreateSimpleEvent, const char *name, SceUInt32 attr, SceUInt32 init_pattern, const SceKernelSimpleEventOptParam *pOptParam);
DECL_EXPORT(int, _sceKernelCreateTimer, const char *name, SceUInt32 attr, const uint32_t *opt_params);
DECL_EXPORT(int, _sceKernelPollEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data);
DECL_EXPORT(int, _sceKernelWaitEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout);
DECL_EXPORT(int, _sceKernelWaitEventCB, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout);
DECL_EXPORT(int, __sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, Ptr<SceKernelCreateLwMutex_opt> opt);
DECL_EXPORT(int, _sceKernelDeleteLwMutex, Ptr<SceKernelLwMutexWork> workarea);
DECL_EXPORT(int, _sceKernelLockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout);
DECL_EXPORT(int, _sceKernelGetLwMutexInfoById, SceUID lightweight_mutex_id, Ptr<SceKernelLwMutexInfo> info, SceSize size);
DECL_EXPORT(SceUID, _sceKernelCreateRWLock, const char *name, SceUInt32 attr, SceKernelMutexOptParam *opt_param);
DECL_EXPORT(SceInt32, _sceKernelLockMutexCB, SceUID mutexId, SceInt32 lockCount, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, _sceKernelLockReadRWLock, SceUID lock_id, SceUInt32 *timeout);
DECL_EXPORT(SceInt32, _sceKernelLockReadRWLockCB, SceUID lock_id, SceUInt32 *timeout);
DECL_EXPORT(SceInt32, _sceKernelLockWriteRWLock, SceUID lock_id, SceUInt32 *timeout);
DECL_EXPORT(SceInt32, _sceKernelLockWriteRWLockCB, SceUID lock_id, SceUInt32 *timeout);
DECL_EXPORT(int, _sceKernelCreateLwCond, Ptr<SceKernelLwCondWork> workarea, const char *name, SceUInt attr, Ptr<SceKernelCreateLwCond_opt> opt);
DECL_EXPORT(SceInt32, _sceKernelGetCallbackInfo, SceUID callbackId, SceKernelCallbackInfo *pInfo);
DECL_EXPORT(int, _sceKernelGetRWLockInfo, SceUID rwlockId, SceKernelRWLockInfo *info);
DECL_EXPORT(SceInt32, _sceKernelGetThreadCpuAffinityMask, SceUID thid);
DECL_EXPORT(SceInt32, _sceKernelWaitLwCondCB, Ptr<SceKernelLwCondWork> pWork, SceUInt32 *pTimeout);
DECL_EXPORT(int, sceKernelCreateThreadForUser, const char *name, SceKernelThreadEntry entry, int init_priority, SceKernelCreateThread_opt *options);
DECL_EXPORT(int, _sceKernelStartThread, SceUID thid, SceSize arglen, Ptr<void> argp);
DECL_EXPORT(SceInt32, _sceKernelGetThreadInfo, SceUID threadId, Ptr<SceKernelThreadInfo> pInfo);
DECL_EXPORT(int, _sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout);
DECL_EXPORT(SceUID, _sceKernelCreateEventFlag, const char *pName, SceUInt32 attr, SceUInt32 initPattern, const SceKernelEventFlagOptParam *pOptParam);
DECL_EXPORT(int, _sceKernelCreateSema, const char *name, SceUInt attr, int initVal, Ptr<SceKernelCreateSema_opt> opt);
DECL_EXPORT(int, _sceKernelGetMutexInfo, SceUID mutexId, SceKernelMutexInfo *pInfo);
DECL_EXPORT(SceInt32, _sceKernelGetSemaInfo, SceUID semaId, Ptr<SceKernelSemaInfo> pInfo);
DECL_EXPORT(SceInt32, _sceKernelWaitCond, SceUID condId, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, _sceKernelWaitCondCB, SceUID condId, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, _sceKernelWaitEventFlagCB, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, _sceKernelWaitSema, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, _sceKernelWaitSemaCB, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout);
DECL_EXPORT(SceInt32, _sceKernelGetEventFlagInfo, SceUID evfId, Ptr<SceKernelEventFlagInfo> pInfo);
DECL_EXPORT(SceInt32, _sceKernelGetEventPattern, SceUID event_id, SceUInt32 *get_pattern);
DECL_EXPORT(int, _sceKernelPollEventFlag, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits);
DECL_EXPORT(SceInt32, _sceKernelCancelEventFlag, SceUID event_id, SceUInt pattern, SceUInt32 *num_wait_thread);
DECL_EXPORT(int, _sceKernelCancelSema, SceUID semaId, SceInt32 setCount, SceUInt32 *pNumWaitThreads);
DECL_EXPORT(int, _sceKernelRegisterThreadEventHandler, const char *name, SceUID thread_mask, SceUInt32 mask, sceKernelRegisterThreadEventHandlerOpt *opt);
DECL_EXPORT(int, _sceKernelWaitThreadEnd, SceUID thid, int *stat, SceUInt *timeout);
DECL_EXPORT(int, _sceKernelWaitThreadEndCB, SceUID thid, int *stat, SceUInt *timeout);
DECL_EXPORT(int, _sceKernelWaitSignal, uint32_t unknown, uint32_t delay, uint32_t timeout);
DECL_EXPORT(int, _sceKernelWaitSignalCB, uint32_t unknown, uint32_t delay, uint32_t timeout);
DECL_EXPORT(int, _sceKernelSetThreadContextForVM, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo);
DECL_EXPORT(int, _sceKernelGetThreadContextForVM, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo);
DECL_EXPORT(int, sceKernelResumeThreadForVM, SceUID threadId);
DECL_EXPORT(int, sceKernelSuspendThreadForVM, SceUID threadId);
