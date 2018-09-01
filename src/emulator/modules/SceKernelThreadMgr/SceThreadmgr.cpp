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

    const ThreadStatePtr thread = lock_and_find(thid, host.kernel.threads, host.kernel.mutex);
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

int delay_thread(SceUInt delay) {
#ifdef _WIN32
    Sleep(delay / 1000);
#else
    usleep(delay);
#endif
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelDelayThread, SceUInt delay) {
    STUBBED("bad accuracy");
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThread200, SceUInt delay) {
    STUBBED("bad accuracy, untested");
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThreadCB, SceUInt delay) {
    STUBBED("no CB, bad accuracy");
    return delay_thread(delay);
}

EXPORT(int, sceKernelDelayThreadCB200, SceUInt delay) {
    STUBBED("no CB, bad accuracy, untested");
    return delay_thread(delay);
}

EXPORT(int, sceKernelDeleteCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteCond) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteEventFlag, SceUID event_id) {
    return eventflag_delete(host.kernel, export_name, thread_id, event_id);
}

EXPORT(int, sceKernelDeleteMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteMutex, SceInt32 mutexid) {
    return mutex_delete(host.kernel, export_name, thread_id, mutexid, SyncWeight::Heavy);
}

EXPORT(int, sceKernelDeleteRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteSimpleEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteThread, SceUID thid) {
    const ThreadStatePtr thread = lock_and_find(thid, host.kernel.threads, host.kernel.mutex);

    const std::lock_guard<std::mutex> lock2(host.kernel.mutex);
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
    const std::lock_guard<std::mutex> lock(thread->mutex);

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

    const std::lock_guard<std::mutex> lock2(host.kernel.mutex);
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

EXPORT(int, sceKernelSendSignal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetEventFlag, SceUID eventid, unsigned int flags) {
    return eventflag_set(host.kernel, export_name, eventid, flags);
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
    return semaphore_signal(host.kernel, export_name, semaid, signal);
}

EXPORT(int, sceKernelStartTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelStopTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryLockMutex) {
    return UNIMPLEMENTED();
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
