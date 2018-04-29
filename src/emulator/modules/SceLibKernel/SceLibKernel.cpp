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

#include "SceLibKernel.h"

#include <cpu/functions.h>
#include <host/functions.h>
#include <rtc/rtc.h>
#include <io/functions.h>
#include <kernel/functions.h>
#include <kernel/thread_functions.h>
#include <util/log.h>

#include <kernel/load_self.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr.h>
#undef st_atime
#undef st_ctime
#undef st_mtime
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/modulemgr.h>

#include <algorithm>
#include <cstdlib>

static const bool LOG_SYNC_PRIMITIVES = false;

// *********
// * Mutex *
// *********

SceUID create_mutex(SceUID *uid_out, HostState &host, SceUID thread_id, MutexPtrs &host_mutexes, const char *name, SceUInt attr, int init_count, bool is_lw) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR("sceKernelCreate(Lw)Mutex", SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }
    if (init_count < 0) {
        return RET_ERROR("sceKernelCreate(Lw)Mutex", SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    }
    if (init_count > 1 && (attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE)) {
        return RET_ERROR("sceKernelCreate(Lw)Mutex", SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    }

    const MutexPtr mutex = std::make_shared<Mutex>();
    mutex->lock_count = init_count;
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, mutex->name);
    mutex->attr = attr;
    mutex->owner = nullptr;
    if (init_count > 0) {
        const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
        mutex->owner = thread;
    }
    const std::lock_guard<std::mutex> lock(host.kernel.mutex);
    const SceUID uid = host.kernel.next_uid++;
    host_mutexes.emplace(uid, mutex);

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("Creating {}: uid:{} thread_id:{} name:\"{}\" attr:{} init_count:{}",
            is_lw ? "lwmutex" : "mutex", uid, thread_id, name, attr, init_count);
    }

    if (uid_out) {
        *uid_out = uid;
    }

    return SCE_KERNEL_OK;
}

int lock_mutex(HostState &host, SceUID thread_id, MutexPtrs &host_mutexes, SceUID mutexid, int lock_count, unsigned int *timeout, bool is_lw) {
    assert(!timeout);

    // TODO Don't lock twice.
    const MutexPtr mutex = lock_and_find(mutexid, host_mutexes, host.kernel.mutex);
    if (!mutex) {
        return RET_ERROR("sceKernelLock(Lw)Mutex", SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("Locking mutex: uid:{} thread_id:{} name:\"{}\" attr:{} lock_count:{}",
            mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count);
    }

    const std::lock_guard<std::mutex> lock(mutex->mutex);

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    bool is_recursive = (mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE);
    bool is_fifo = (mutex->attr & SCE_KERNEL_ATTR_TH_FIFO);

    if (mutex->lock_count > 0) {
        // Already owned

        if (mutex->owner == thread) {
            // Owned by ourselves

            if (is_recursive) {
                mutex->lock_count += lock_count;
                return SCE_KERNEL_OK;
            } else {
                return SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
            }
        } else {
            // Owned by someone else
            // Sleep thread!

            const std::lock_guard<std::mutex> lock2(thread->mutex);
            assert(thread->to_do == ThreadToDo::run);
            thread->to_do = ThreadToDo::wait;
            mutex->waiting_threads.emplace(thread, lock_count, is_fifo ? 0 : thread->priority);
            stop(*thread->cpu);
        }
    } else {
        // Not owned
        // Take ownership!

        mutex->lock_count += lock_count;
        mutex->owner = thread;
    }

    return SCE_KERNEL_OK;
}

int unlock_mutex(HostState &host, SceUID thread_id, MutexPtrs &host_mutexes, SceUID mutexid, int unlock_count) {
    const MutexPtr mutex = lock_and_find(mutexid, host_mutexes, host.kernel.mutex);
    if (!mutex) {
        return RET_ERROR("sceKernelUnlock(Lw)Mutex", SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    }

    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("Unlocking mutex: uid:{} thread_id:{} name:\"{}\" attr:{} lock_count:{}",
            mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count, unlock_count);
    }

    const std::lock_guard<std::mutex> lock(mutex->mutex);

    const ThreadStatePtr cur_thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    if (cur_thread == mutex->owner) {
        if (unlock_count > mutex->lock_count) {
            return RET_ERROR("sceKernelUnlock(Lw)Mutex", SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF);
        }
        mutex->lock_count -= unlock_count;
        if (mutex->lock_count == 0) {
            mutex->owner = nullptr;
            if (mutex->waiting_threads.size() > 0) {
                const auto waiting_thread_data = mutex->waiting_threads.top();
                const auto waiting_thread = waiting_thread_data.thread;
                const auto waiting_lock_count = waiting_thread_data.lock_count;

                assert(waiting_thread->to_do == ThreadToDo::wait);
                waiting_thread->to_do = ThreadToDo::run;
                mutex->waiting_threads.pop();
                mutex->lock_count += waiting_lock_count;
                mutex->owner = waiting_thread;
                waiting_thread->something_to_do.notify_one();
            }
        }
    }

    return SCE_KERNEL_OK;
}

int delete_mutex(HostState &host, SceUID thread_id, MutexPtrs &host_mutexes, SceUID mutexid) {
    const MutexPtr mutex = lock_and_find(mutexid, host_mutexes, host.kernel.mutex);
    if (!mutex) {
        return RET_ERROR("sceKernel(Lw)UnlockMutex", SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    }
    if (LOG_SYNC_PRIMITIVES) {
        LOG_DEBUG("Deleting mutex: uid:{} thread_id:{} name:\"{}\" attr:{} lock_count:{} waiting_threads:{}",
            mutexid, thread_id, mutex->name, mutex->attr, mutex->lock_count, mutex->waiting_threads.size());
    }

    if (mutex->waiting_threads.empty()) {
        const std::lock_guard<std::mutex> lock(mutex->mutex);
        host_mutexes.erase(mutexid);
    } else {
        // TODO:
    }

    return SCE_KERNEL_OK;
}

// **************
// * Sempaphore *
// **************

SceUID create_semaphore(HostState& host, const char* name, SceUInt attr, int initVal, int maxVal) {
    if ((strlen(name) > 31) && ((attr & 0x80) == 0x80)) {
        return RET_ERROR("sceKernelCreateSema", SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    }

    const SemaphorePtr semaphore = std::make_shared<Semaphore>();
    semaphore->val = initVal;
    semaphore->max = maxVal;
    semaphore->attr = attr;
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, semaphore->name);
    const std::lock_guard<std::mutex> lock(host.kernel.mutex);
    const SceUID uid = host.kernel.next_uid++;
    host.kernel.semaphores.emplace(uid, semaphore);

    return uid;
}

int wait_semaphore(HostState& host, SceUID thread_id, SceUID semaid, int signal, SceUInt* timeout)
{
    assert(semaid >= 0);
    assert(signal == 1);
    assert(timeout == nullptr);

    // TODO Don't lock twice.
    const SemaphorePtr semaphore = lock_and_find(semaid, host.kernel.semaphores, host.kernel.mutex);
    if (!semaphore) {
        return RET_ERROR("sceKernelWaitSema", SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    const std::lock_guard<std::mutex> lock(semaphore->mutex);

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    bool is_fifo = (semaphore->attr & SCE_KERNEL_ATTR_TH_FIFO);

    if (semaphore->val <= 0) {
        const std::lock_guard<std::mutex> lock2(thread->mutex);
        assert(thread->to_do == ThreadToDo::run);
        thread->to_do = ThreadToDo::wait;
        semaphore->waiting_threads.emplace(thread, signal, is_fifo ? 0 : thread->priority);
        stop(*thread->cpu);
    }
    else {
        semaphore->val -= signal;
    }

    return SCE_KERNEL_OK;
}

int signal_sema(HostState& host, SceUID semaid, int signal) {
    // TODO Don't lock twice.
    const SemaphorePtr semaphore = lock_and_find(semaid, host.kernel.semaphores, host.kernel.mutex);
    if (!semaphore) {
        return RET_ERROR("sceKernelSignalSema", SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }

    const std::lock_guard<std::mutex> lock(semaphore->mutex);

    if (semaphore->val + signal > semaphore->max) {
        return RET_ERROR("sceKernelSignalSema", SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF);
    }
    semaphore->val += signal;

    while (semaphore->val > 0 && semaphore->waiting_threads.size() > 0) {
        const auto waiting_thread_data = semaphore->waiting_threads.top();
        const auto waiting_thread = waiting_thread_data.thread;
        const auto waiting_signal_count = waiting_thread_data.lock_count;

        assert(waiting_thread->to_do == ThreadToDo::wait);
        waiting_thread->to_do = ThreadToDo::run;
        semaphore->waiting_threads.pop();
        semaphore->val -= waiting_signal_count;
        waiting_thread->something_to_do.notify_one();
    }

    return 0;
}

EXPORT(int, SceKernelStackChkGuard) {
    return unimplemented("SceKernelStackChkGuard");
}

EXPORT(int, __sce_aeabi_idiv0) {
    return unimplemented("__sce_aeabi_idiv0");
}

EXPORT(int, __sce_aeabi_ldiv0) {
    return unimplemented("__sce_aeabi_ldiv0");
}

EXPORT(int, __stack_chk_fail) {
    return unimplemented("__stack_chk_fail");
}

EXPORT(int, __stack_chk_guard) {
    return unimplemented("__stack_chk_guard");
}

EXPORT(int, _sceKernelCreateLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, int init_count, const SceKernelLwMutexOptParam *opt_param) {
    assert(name != nullptr);
    assert(init_count >= 0);
    assert(opt_param == nullptr);

    auto uid_out = &workarea.get(host.mem)->uid;
    return create_mutex(uid_out, host, thread_id, host.kernel.lwmutexes, name, attr, init_count, true);
}

EXPORT(int, sceClibAbort) {
    return unimplemented("sceClibAbort");
}

EXPORT(int, sceClibDprintf) {
    return unimplemented("sceClibDprintf");
}

EXPORT(int, sceClibLookCtypeTable) {
    return unimplemented("sceClibLookCtypeTable");
}

EXPORT(int, sceClibMemchr) {
    return unimplemented("sceClibMemchr");
}

EXPORT(int, sceClibMemcmp) {
    return unimplemented("sceClibMemcmp");
}

EXPORT(int, sceClibMemcmpConstTime) {
    return unimplemented("sceClibMemcmpConstTime");
}

EXPORT(int, sceClibMemcpy) {
    return unimplemented("sceClibMemcpy");
}

EXPORT(int, sceClibMemcpyChk) {
    return unimplemented("sceClibMemcpyChk");
}

EXPORT(int, sceClibMemcpy_safe) {
    return unimplemented("sceClibMemcpy_safe");
}

EXPORT(int, sceClibMemmove) {
    return unimplemented("sceClibMemmove");
}

EXPORT(int, sceClibMemmoveChk) {
    return unimplemented("sceClibMemmoveChk");
}

EXPORT(Ptr<void>, sceClibMemset, Ptr<void> s, int c, SceSize n) {
    memset(s.get(host.mem), c, n);
    return s;
}

EXPORT(int, sceClibMemsetChk) {
    return unimplemented("sceClibMemsetChk");
}

EXPORT(int, sceClibMspaceCalloc) {
    return unimplemented("sceClibMspaceCalloc");
}

EXPORT(int, sceClibMspaceCreate) {
    return unimplemented("sceClibMspaceCreate");
}

EXPORT(int, sceClibMspaceDestroy) {
    return unimplemented("sceClibMspaceDestroy");
}

EXPORT(int, sceClibMspaceFree) {
    return unimplemented("sceClibMspaceFree");
}

EXPORT(int, sceClibMspaceIsHeapEmpty) {
    return unimplemented("sceClibMspaceIsHeapEmpty");
}

EXPORT(int, sceClibMspaceMalloc) {
    return unimplemented("sceClibMspaceMalloc");
}

EXPORT(int, sceClibMspaceMallocStats) {
    return unimplemented("sceClibMspaceMallocStats");
}

EXPORT(int, sceClibMspaceMallocStatsFast) {
    return unimplemented("sceClibMspaceMallocStatsFast");
}

EXPORT(int, sceClibMspaceMallocUsableSize) {
    return unimplemented("sceClibMspaceMallocUsableSize");
}

EXPORT(int, sceClibMspaceMemalign) {
    return unimplemented("sceClibMspaceMemalign");
}

EXPORT(int, sceClibMspaceRealloc) {
    return unimplemented("sceClibMspaceRealloc");
}

EXPORT(int, sceClibMspaceReallocalign) {
    return unimplemented("sceClibMspaceReallocalign");
}

EXPORT(int, sceClibPrintf, const char *format, void *args) {
    // TODO args
    LOG_INFO("{}", format);
    return SCE_KERNEL_OK;
}

EXPORT(int, sceClibSnprintf, char *dest, SceSize size, const char *format, void *args) {
    // TODO args
    return snprintf(dest, size, "%s", format);
}

EXPORT(int, sceClibSnprintfChk) {
    return unimplemented("sceClibSnprintfChk");
}

EXPORT(int, sceClibStrcatChk) {
    return unimplemented("sceClibStrcatChk");
}

EXPORT(int, sceClibStrchr) {
    return unimplemented("sceClibStrchr");
}

EXPORT(int, sceClibStrcmp) {
    return unimplemented("sceClibStrcmp");
}

EXPORT(int, sceClibStrcpyChk) {
    return unimplemented("sceClibStrcpyChk");
}

EXPORT(int, sceClibStrlcat) {
    return unimplemented("sceClibStrlcat");
}

EXPORT(int, sceClibStrlcatChk) {
    return unimplemented("sceClibStrlcatChk");
}

EXPORT(int, sceClibStrlcpy) {
    return unimplemented("sceClibStrlcpy");
}

EXPORT(int, sceClibStrlcpyChk) {
    return unimplemented("sceClibStrlcpyChk");
}

EXPORT(int, sceClibStrncasecmp) {
    return unimplemented("sceClibStrncasecmp");
}

EXPORT(int, sceClibStrncat) {
    return unimplemented("sceClibStrncat");
}

EXPORT(int, sceClibStrncatChk) {
    return unimplemented("sceClibStrncatChk");
}

EXPORT(int, sceClibStrncmp) {
    return unimplemented("sceClibStrncmp");
}

EXPORT(int, sceClibStrncpy) {
    return unimplemented("sceClibStrncpy");
}

EXPORT(int, sceClibStrncpyChk) {
    return unimplemented("sceClibStrncpyChk");
}

EXPORT(int, sceClibStrnlen) {
    return unimplemented("sceClibStrnlen");
}

EXPORT(int, sceClibStrrchr) {
    return unimplemented("sceClibStrrchr");
}

EXPORT(int, sceClibStrstr) {
    return unimplemented("sceClibStrstr");
}

EXPORT(int, sceClibStrtoll) {
    return unimplemented("sceClibStrtoll");
}

EXPORT(int, sceClibTolower) {
    return unimplemented("sceClibTolower");
}

EXPORT(int, sceClibToupper) {
    return unimplemented("sceClibToupper");
}

EXPORT(int, sceClibVdprintf) {
    return unimplemented("sceClibVdprintf");
}

EXPORT(int, sceClibVprintf) {
    return unimplemented("sceClibVprintf");
}

EXPORT(int, sceClibVsnprintf) {
    return unimplemented("sceClibVsnprintf");
}

EXPORT(int, sceClibVsnprintfChk) {
    return unimplemented("sceClibVsnprintfChk");
}

EXPORT(int, sceIoChstat) {
    return unimplemented("sceIoChstat");
}

EXPORT(int, sceIoChstatAsync) {
    return unimplemented("sceIoChstatAsync");
}

EXPORT(int, sceIoChstatByFd) {
    return unimplemented("sceIoChstatByFd");
}

EXPORT(int, sceIoCompleteMultiple) {
    return unimplemented("sceIoCompleteMultiple");
}

EXPORT(int, sceIoDevctl) {
    return unimplemented("sceIoDevctl");
}

EXPORT(int, sceIoDopen, const char *dir) {
    return open_dir(host.io, dir, host.pref_path.c_str());
}

EXPORT(int, sceIoDevctlAsync) {
    return unimplemented("sceIoDevctlAsync");
}

EXPORT(int, sceIoDread, SceUID fd, SceIoDirent *dir) {
    return read_dir(host.io, fd, dir);
}

EXPORT(int, sceIoGetstat, const char *file, SceIoStat *stat) {
    return stat_file(file, stat, host.pref_path.c_str(), host.io.vpk.get(), host.kernel.base_tick.tick);
}

EXPORT(int, sceIoGetstatAsync) {
    return unimplemented("sceIoGetstatAsync");
}

EXPORT(int, sceIoGetstatByFd) {
    return unimplemented("sceIoGetstatByFd");
}

EXPORT(int, sceIoIoctl) {
    return unimplemented("sceIoIoctl");
}

EXPORT(int, sceIoIoctlAsync) {
    return unimplemented("sceIoIoctlAsync");
}

EXPORT(int, sceIoLseek, SceUID fd, SceOff offset, int whence) {
    return seek_file(fd, offset, whence, host.io);
}

EXPORT(int, sceIoMkdir, const char *dir, SceMode mode) {
    return create_dir(dir, mode, host.pref_path.c_str());
}

EXPORT(int, sceIoMkdirAsync) {
    return unimplemented("sceIoMkdirAsync");
}

EXPORT(int, sceIoLseekAsync) {
    return unimplemented("sceIoLseekAsync");
}

EXPORT(SceUID, sceIoOpen, const char *file, int flags, SceMode mode) {
    if (file == nullptr) {
        return RET_ERROR("sceIoOpen", 0x80010016); // SCE_ERROR_ERRNO_EINVAL, missing in vita-headers
    }
    LOG_INFO("Opening file: {}", file);
    return open_file(host.io, file, flags, host.pref_path.c_str());
}

EXPORT(int, sceIoOpenAsync) {
    return unimplemented("sceIoOpenAsync");
}

EXPORT(int, sceIoPread, SceUID fd, void *data, SceSize size, SceOff offset) {
    seek_file(fd, offset, SEEK_SET, host.io);
    return read_file(data, host.io, fd, size);
}

EXPORT(int, sceIoPreadAsync) {
    return unimplemented("sceIoPreadAsync");
}

EXPORT(int, sceIoPwrite, SceUID fd, const void *data, SceSize size, SceOff offset) {
    seek_file(fd, offset, SEEK_SET, host.io);
    return write_file(fd, data, size, host.io);
}

EXPORT(int, sceIoPwriteAsync) {
    return unimplemented("sceIoPwriteAsync");
}

EXPORT(int, sceIoRemove, const char *path) {
    if (path == nullptr) {
        return RET_ERROR("sceIoRemove", 0x80010016); // SCE_ERROR_ERRNO_EINVAL, missing in vita-headers
    }
    return remove_file(path, host.pref_path.c_str());
}

EXPORT(int, sceIoRemoveAsync) {
    return unimplemented("sceIoRemoveAsync");
}

EXPORT(int, sceIoRename) {
    return unimplemented("sceIoRename");
}

EXPORT(int, sceIoRenameAsync) {
    return unimplemented("sceIoRenameAsync");
}

EXPORT(int, sceIoRmdir, const char *path) {
    if (path == nullptr) {
        return RET_ERROR("sceIoRmdir", 0x80010016); // SCE_ERROR_ERRNO_EINVAL, missing in vita-headers
    }
    return remove_dir(path, host.pref_path.c_str());
}

EXPORT(int, sceIoRmdirAsync) {
    return unimplemented("sceIoRmdirAsync");
}

EXPORT(int, sceIoSync) {
    return unimplemented("sceIoSync");
}

EXPORT(int, sceIoSyncAsync) {
    return unimplemented("sceIoSyncAsync");
}

EXPORT(int, sceKernelAtomicAddAndGet16) {
    return unimplemented("sceKernelAtomicAddAndGet16");
}

EXPORT(int, sceKernelAtomicAddAndGet32) {
    return unimplemented("sceKernelAtomicAddAndGet32");
}

EXPORT(int, sceKernelAtomicAddAndGet64) {
    return unimplemented("sceKernelAtomicAddAndGet64");
}

EXPORT(int, sceKernelAtomicAddAndGet8) {
    return unimplemented("sceKernelAtomicAddAndGet8");
}

EXPORT(int, sceKernelAtomicAddUnless16) {
    return unimplemented("sceKernelAtomicAddUnless16");
}

EXPORT(int, sceKernelAtomicAddUnless32) {
    return unimplemented("sceKernelAtomicAddUnless32");
}

EXPORT(int, sceKernelAtomicAddUnless64) {
    return unimplemented("sceKernelAtomicAddUnless64");
}

EXPORT(int, sceKernelAtomicAddUnless8) {
    return unimplemented("sceKernelAtomicAddUnless8");
}

EXPORT(int, sceKernelAtomicAndAndGet16) {
    return unimplemented("sceKernelAtomicAndAndGet16");
}

EXPORT(int, sceKernelAtomicAndAndGet32) {
    return unimplemented("sceKernelAtomicAndAndGet32");
}

EXPORT(int, sceKernelAtomicAndAndGet64) {
    return unimplemented("sceKernelAtomicAndAndGet64");
}

EXPORT(int, sceKernelAtomicAndAndGet8) {
    return unimplemented("sceKernelAtomicAndAndGet8");
}

EXPORT(int, sceKernelAtomicClearAndGet16) {
    return unimplemented("sceKernelAtomicClearAndGet16");
}

EXPORT(int, sceKernelAtomicClearAndGet32) {
    return unimplemented("sceKernelAtomicClearAndGet32");
}

EXPORT(int, sceKernelAtomicClearAndGet64) {
    return unimplemented("sceKernelAtomicClearAndGet64");
}

EXPORT(int, sceKernelAtomicClearAndGet8) {
    return unimplemented("sceKernelAtomicClearAndGet8");
}

EXPORT(int, sceKernelAtomicClearMask16) {
    return unimplemented("sceKernelAtomicClearMask16");
}

EXPORT(int, sceKernelAtomicClearMask32) {
    return unimplemented("sceKernelAtomicClearMask32");
}

EXPORT(int, sceKernelAtomicClearMask64) {
    return unimplemented("sceKernelAtomicClearMask64");
}

EXPORT(int, sceKernelAtomicClearMask8) {
    return unimplemented("sceKernelAtomicClearMask8");
}

EXPORT(int, sceKernelAtomicCompareAndSet16) {
    return unimplemented("sceKernelAtomicCompareAndSet16");
}

EXPORT(int, sceKernelAtomicCompareAndSet32) {
    return unimplemented("sceKernelAtomicCompareAndSet32");
}

EXPORT(int, sceKernelAtomicCompareAndSet64) {
    return unimplemented("sceKernelAtomicCompareAndSet64");
}

EXPORT(int, sceKernelAtomicCompareAndSet8) {
    return unimplemented("sceKernelAtomicCompareAndSet8");
}

EXPORT(int, sceKernelAtomicDecIfPositive16) {
    return unimplemented("sceKernelAtomicDecIfPositive16");
}

EXPORT(int, sceKernelAtomicDecIfPositive32) {
    return unimplemented("sceKernelAtomicDecIfPositive32");
}

EXPORT(int, sceKernelAtomicDecIfPositive64) {
    return unimplemented("sceKernelAtomicDecIfPositive64");
}

EXPORT(int, sceKernelAtomicDecIfPositive8) {
    return unimplemented("sceKernelAtomicDecIfPositive8");
}

EXPORT(int, sceKernelAtomicGetAndAdd16) {
    return unimplemented("sceKernelAtomicGetAndAdd16");
}

EXPORT(int, sceKernelAtomicGetAndAdd32) {
    return unimplemented("sceKernelAtomicGetAndAdd32");
}

EXPORT(int, sceKernelAtomicGetAndAdd64) {
    return unimplemented("sceKernelAtomicGetAndAdd64");
}

EXPORT(int, sceKernelAtomicGetAndAdd8) {
    return unimplemented("sceKernelAtomicGetAndAdd8");
}

EXPORT(int, sceKernelAtomicGetAndAnd16) {
    return unimplemented("sceKernelAtomicGetAndAnd16");
}

EXPORT(int, sceKernelAtomicGetAndAnd32) {
    return unimplemented("sceKernelAtomicGetAndAnd32");
}

EXPORT(int, sceKernelAtomicGetAndAnd64) {
    return unimplemented("sceKernelAtomicGetAndAnd64");
}

EXPORT(int, sceKernelAtomicGetAndAnd8) {
    return unimplemented("sceKernelAtomicGetAndAnd8");
}

EXPORT(int, sceKernelAtomicGetAndClear16) {
    return unimplemented("sceKernelAtomicGetAndClear16");
}

EXPORT(int, sceKernelAtomicGetAndClear32) {
    return unimplemented("sceKernelAtomicGetAndClear32");
}

EXPORT(int, sceKernelAtomicGetAndClear64) {
    return unimplemented("sceKernelAtomicGetAndClear64");
}

EXPORT(int, sceKernelAtomicGetAndClear8) {
    return unimplemented("sceKernelAtomicGetAndClear8");
}

EXPORT(int, sceKernelAtomicGetAndOr16) {
    return unimplemented("sceKernelAtomicGetAndOr16");
}

EXPORT(int, sceKernelAtomicGetAndOr32) {
    return unimplemented("sceKernelAtomicGetAndOr32");
}

EXPORT(int, sceKernelAtomicGetAndOr64) {
    return unimplemented("sceKernelAtomicGetAndOr64");
}

EXPORT(int, sceKernelAtomicGetAndOr8) {
    return unimplemented("sceKernelAtomicGetAndOr8");
}

EXPORT(int, sceKernelAtomicGetAndSet16) {
    return unimplemented("sceKernelAtomicGetAndSet16");
}

EXPORT(int, sceKernelAtomicGetAndSet32) {
    return unimplemented("sceKernelAtomicGetAndSet32");
}

EXPORT(int, sceKernelAtomicGetAndSet64) {
    return unimplemented("sceKernelAtomicGetAndSet64");
}

EXPORT(int, sceKernelAtomicGetAndSet8) {
    return unimplemented("sceKernelAtomicGetAndSet8");
}

EXPORT(int, sceKernelAtomicGetAndSub16) {
    return unimplemented("sceKernelAtomicGetAndSub16");
}

EXPORT(int, sceKernelAtomicGetAndSub32) {
    return unimplemented("sceKernelAtomicGetAndSub32");
}

EXPORT(int, sceKernelAtomicGetAndSub64) {
    return unimplemented("sceKernelAtomicGetAndSub64");
}

EXPORT(int, sceKernelAtomicGetAndSub8) {
    return unimplemented("sceKernelAtomicGetAndSub8");
}

EXPORT(int, sceKernelAtomicGetAndXor16) {
    return unimplemented("sceKernelAtomicGetAndXor16");
}

EXPORT(int, sceKernelAtomicGetAndXor32) {
    return unimplemented("sceKernelAtomicGetAndXor32");
}

EXPORT(int, sceKernelAtomicGetAndXor64) {
    return unimplemented("sceKernelAtomicGetAndXor64");
}

EXPORT(int, sceKernelAtomicGetAndXor8) {
    return unimplemented("sceKernelAtomicGetAndXor8");
}

EXPORT(int, sceKernelAtomicOrAndGet16) {
    return unimplemented("sceKernelAtomicOrAndGet16");
}

EXPORT(int, sceKernelAtomicOrAndGet32) {
    return unimplemented("sceKernelAtomicOrAndGet32");
}

EXPORT(int, sceKernelAtomicOrAndGet64) {
    return unimplemented("sceKernelAtomicOrAndGet64");
}

EXPORT(int, sceKernelAtomicOrAndGet8) {
    return unimplemented("sceKernelAtomicOrAndGet8");
}

EXPORT(int, sceKernelAtomicSet16) {
    return unimplemented("sceKernelAtomicSet16");
}

EXPORT(int, sceKernelAtomicSet32) {
    return unimplemented("sceKernelAtomicSet32");
}

EXPORT(int, sceKernelAtomicSet64) {
    return unimplemented("sceKernelAtomicSet64");
}

EXPORT(int, sceKernelAtomicSet8) {
    return unimplemented("sceKernelAtomicSet8");
}

EXPORT(int, sceKernelAtomicSubAndGet16) {
    return unimplemented("sceKernelAtomicSubAndGet16");
}

EXPORT(int, sceKernelAtomicSubAndGet32) {
    return unimplemented("sceKernelAtomicSubAndGet32");
}

EXPORT(int, sceKernelAtomicSubAndGet64) {
    return unimplemented("sceKernelAtomicSubAndGet64");
}

EXPORT(int, sceKernelAtomicSubAndGet8) {
    return unimplemented("sceKernelAtomicSubAndGet8");
}

EXPORT(int, sceKernelAtomicXorAndGet16) {
    return unimplemented("sceKernelAtomicXorAndGet16");
}

EXPORT(int, sceKernelAtomicXorAndGet32) {
    return unimplemented("sceKernelAtomicXorAndGet32");
}

EXPORT(int, sceKernelAtomicXorAndGet64) {
    return unimplemented("sceKernelAtomicXorAndGet64");
}

EXPORT(int, sceKernelAtomicXorAndGet8) {
    return unimplemented("sceKernelAtomicXorAndGet8");
}

EXPORT(int, sceKernelBacktrace) {
    return unimplemented("sceKernelBacktrace");
}

EXPORT(int, sceKernelBacktraceSelf) {
    return unimplemented("sceKernelBacktraceSelf");
}

EXPORT(int, sceKernelCallModuleExit) {
    return unimplemented("sceKernelCallModuleExit");
}

EXPORT(int, sceKernelCallWithChangeStack) {
    return unimplemented("sceKernelCallWithChangeStack");
}

EXPORT(int, sceKernelCancelEvent) {
    return unimplemented("sceKernelCancelEvent");
}

EXPORT(int, sceKernelCancelEventFlag) {
    return unimplemented("sceKernelCancelEventFlag");
}

EXPORT(int, sceKernelCancelEventWithSetPattern) {
    return unimplemented("sceKernelCancelEventWithSetPattern");
}

EXPORT(int, sceKernelCancelMsgPipe) {
    return unimplemented("sceKernelCancelMsgPipe");
}

EXPORT(int, sceKernelCancelMutex) {
    return unimplemented("sceKernelCancelMutex");
}

EXPORT(int, sceKernelCancelRWLock) {
    return unimplemented("sceKernelCancelRWLock");
}

EXPORT(int, sceKernelCancelSema) {
    return unimplemented("sceKernelCancelSema");
}

EXPORT(int, sceKernelCancelTimer) {
    return unimplemented("sceKernelCancelTimer");
}

EXPORT(int, sceKernelChangeCurrentThreadAttr) {
    return unimplemented("sceKernelChangeCurrentThreadAttr");
}

EXPORT(int, sceKernelCheckThreadStack) {
    return unimplemented("sceKernelCheckThreadStack");
}

EXPORT(int, sceKernelCloseModule) {
    return unimplemented("sceKernelCloseModule");
}

EXPORT(int, sceKernelCreateCond) {
    return unimplemented("sceKernelCreateCond");
}

EXPORT(int, sceKernelCreateEventFlag) {
    return unimplemented("sceKernelCreateEventFlag");
}

EXPORT(int, sceKernelCreateLwCond) {
    return unimplemented("sceKernelCreateLwCond");
}

EXPORT(int, sceKernelCreateLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, int init_count, const SceKernelLwMutexOptParam *opt_param) {
    assert(workarea);
    assert(name);
    assert(init_count >= 0);
    assert(opt_param == nullptr);

    auto uid_out = &workarea.get(host.mem)->uid;
    return create_mutex(uid_out, host, thread_id, host.kernel.lwmutexes, name, attr, init_count, true);
}

EXPORT(int, sceKernelCreateMsgPipe) {
    return unimplemented("sceKernelCreateMsgPipe");
}

EXPORT(int, sceKernelCreateMsgPipeWithLR) {
    return unimplemented("sceKernelCreateMsgPipeWithLR");
}

EXPORT(int, sceKernelCreateMutex, const char *name, SceUInt attr, int init_count, SceKernelMutexOptParam *opt_param) {
    SceUID uid;

    if (auto error = create_mutex(&uid, host, thread_id, host.kernel.mutexes, name, attr, init_count, false)) {
        return error;
    }
    return uid;
}

EXPORT(int, sceKernelCreateRWLock) {
    return unimplemented("sceKernelCreateRWLock");
}

EXPORT(SceUID, sceKernelCreateSema, const char *name, SceUInt attr, int initVal, int maxVal, SceKernelSemaOptParam *option) {
    return create_semaphore(host, name, attr, initVal, maxVal);
}

EXPORT(int, sceKernelCreateSema_16XX) {
    return unimplemented("sceKernelCreateSema_16XX");
}

EXPORT(int, sceKernelCreateSimpleEvent) {
    return unimplemented("sceKernelCreateSimpleEvent");
}

EXPORT(SceUID, sceKernelCreateThread, const char *name, emu::SceKernelThreadEntry entry, int init_priority, int stack_size, SceUInt attr, int cpu_affinity_mask, const SceKernelThreadOptParam *option) {
    if (cpu_affinity_mask > 0x70000) {
        return RET_ERROR("sceKernelCreateThread", SCE_KERNEL_ERROR_INVALID_CPU_AFFINITY);
    }
    const CallImport call_import = [&host](uint32_t nid, SceUID thread_id) {
        ::call_import(host, nid, thread_id);
    };

    const SceUID thid = create_thread(entry.cast<const void>(), host.kernel, host.mem, name, init_priority, stack_size, call_import, false);
    if (thid < 0)
        return RET_ERROR("sceKernelCreateThread", thid);
    return thid;
}

EXPORT(int, sceKernelCreateTimer) {
    return unimplemented("sceKernelCreateTimer");
}

EXPORT(int, sceKernelDeleteLwCond) {
    return unimplemented("sceKernelDeleteLwCond");
}

EXPORT(int, sceKernelDeleteLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return delete_mutex(host, thread_id, host.kernel.lwmutexes, lwmutexid);
}

EXPORT(int, sceKernelExitProcess, int res) {
    // TODO Handle exit code?
    stop_all_threads(host.kernel);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetCallbackInfo) {
    return unimplemented("sceKernelGetCallbackInfo");
}

EXPORT(int, sceKernelGetCondInfo) {
    return unimplemented("sceKernelGetCondInfo");
}

EXPORT(int, sceKernelGetCurrentThreadVfpException) {
    return unimplemented("sceKernelGetCurrentThreadVfpException");
}

EXPORT(int, sceKernelGetEventFlagInfo) {
    return unimplemented("sceKernelGetEventFlagInfo");
}

EXPORT(int, sceKernelGetEventInfo) {
    return unimplemented("sceKernelGetEventInfo");
}

EXPORT(int, sceKernelGetEventPattern) {
    return unimplemented("sceKernelGetEventPattern");
}

EXPORT(int, sceKernelGetLwCondInfo) {
    return unimplemented("sceKernelGetLwCondInfo");
}

EXPORT(int, sceKernelGetLwCondInfoById) {
    return unimplemented("sceKernelGetLwCondInfoById");
}

EXPORT(int, sceKernelGetLwMutexInfo) {
    return unimplemented("sceKernelGetLwMutexInfo");
}

EXPORT(int, sceKernelGetLwMutexInfoById) {
    return unimplemented("sceKernelGetLwMutexInfoById");
}

EXPORT(int, sceKernelGetModuleInfoByAddr) {
    return unimplemented("sceKernelGetModuleInfoByAddr");
}

EXPORT(int, sceKernelGetMsgPipeInfo) {
    return unimplemented("sceKernelGetMsgPipeInfo");
}

EXPORT(int, sceKernelGetMutexInfo) {
    return unimplemented("sceKernelGetMutexInfo");
}

EXPORT(int, sceKernelGetOpenPsId) {
    return unimplemented("sceKernelGetOpenPsId");
}

EXPORT(int, sceKernelGetPMUSERENR) {
    return unimplemented("sceKernelGetPMUSERENR");
}

EXPORT(SceUInt64, sceKernelGetProcessTime) {
    return rtc_get_ticks(host.kernel.base_tick.tick);
}

EXPORT(SceUInt32, sceKernelGetProcessTimeLow) {
    return static_cast<SceUInt32>(rtc_get_ticks(host.kernel.base_tick.tick));
}

EXPORT(SceUInt64, sceKernelGetProcessTimeWide) {
    return rtc_get_ticks(host.kernel.base_tick.tick);
}

EXPORT(int, sceKernelGetRWLockInfo) {
    return unimplemented("sceKernelGetRWLockInfo");
}

EXPORT(int, sceKernelGetSemaInfo) {
    return unimplemented("sceKernelGetSemaInfo");
}

EXPORT(int, sceKernelGetSystemInfo) {
    return unimplemented("sceKernelGetSystemInfo");
}

EXPORT(int, sceKernelGetSystemTime) {
    return unimplemented("sceKernelGetSystemTime");
}

EXPORT(Ptr<Ptr<void>>, sceKernelGetTLSAddr, int key) {
    return get_thread_tls_addr(host.kernel, host.mem, thread_id, key);
}

EXPORT(int, sceKernelGetThreadContextForVM) {
    return unimplemented("sceKernelGetThreadContextForVM");
}

EXPORT(int, sceKernelGetThreadCpuAffinityMask) {
    return unimplemented("sceKernelGetThreadCpuAffinityMask");
}

EXPORT(int, sceKernelGetThreadCurrentPriority) {
    return unimplemented("sceKernelGetThreadCurrentPriority");
}

EXPORT(int, sceKernelGetThreadEventInfo) {
    return unimplemented("sceKernelGetThreadEventInfo");
}

EXPORT(int, sceKernelGetThreadExitStatus) {
    return unimplemented("sceKernelGetThreadExitStatus");
}

EXPORT(int, sceKernelGetThreadId) {
    return thread_id;
}

EXPORT(int, sceKernelGetThreadInfo) {
    return unimplemented("sceKernelGetThreadInfo");
}

EXPORT(int, sceKernelGetThreadRunStatus) {
    return unimplemented("sceKernelGetThreadRunStatus");
}

EXPORT(int, sceKernelGetTimerBase) {
    return unimplemented("sceKernelGetTimerBase");
}

EXPORT(int, sceKernelGetTimerEventRemainingTime) {
    return unimplemented("sceKernelGetTimerEventRemainingTime");
}

EXPORT(int, sceKernelGetTimerInfo) {
    return unimplemented("sceKernelGetTimerInfo");
}

EXPORT(int, sceKernelGetTimerTime) {
    return unimplemented("sceKernelGetTimerTime");
}

EXPORT(int, sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option) {
    SceUID file = open_file(host.io, path, SCE_O_RDONLY, host.pref_path.c_str());
    int size = seek_file(file, 0, SEEK_END, host.io);
    void *data = malloc(size);
    Ptr<const void> entry_point;

    SceUID modId = load_self(entry_point, host.kernel, host.mem, data, path);
    close_file(host.io, file);
    free(data);
    if (modId < 0) {
        return RET_ERROR("sceKernelLoadModule", modId);
    };
    return modId;
}

EXPORT(int, sceKernelLoadStartModule, char *path, SceSize args, Ptr<void> argp, int flags, SceKernelLMOption *option, int *status) {
    SceUID file = open_file(host.io, path, SCE_O_RDONLY, host.pref_path.c_str());
    if (file < 0)
        return RET_ERROR("sceKernelLoadStartModule", file);
    int size = seek_file(file, 0, SCE_SEEK_END, host.io);
    if (size < 0)
        return RET_ERROR("sceKernelLoadStartModule", size);
    void *data = malloc(size);
    if (seek_file(file, 0, SCE_SEEK_SET, host.io) < 0)
        return RET_ERROR("sceKernelLoadStartModule", size);
    if (read_file(data, host.io, file, size) < 0) {
        return RET_ERROR("sceKernelLoadStartModule", size);
    };

    Ptr<const void> entry_point;
    SceUID modId = load_self(entry_point, host.kernel, host.mem, data, path);
    close_file(host.io, file);
    free(data);
    if (modId < 0) {
        return RET_ERROR("sceKernelLoadStartModule", modId);
    };

    const SceKernelModuleInfoPtrs::const_iterator module = host.kernel.loaded_modules.find(modId);
    assert(module != host.kernel.loaded_modules.end());

    const CallImport call_import = [&host](uint32_t nid, SceUID thread_id) {
        ::call_import(host, nid, thread_id);
    };

    const SceUID thid = create_thread(entry_point.cast<const void>(), host.kernel, host.mem, module->second.get()->module_name, SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_STACK_SIZE_USER_DEFAULT, call_import, false);

    const ThreadStatePtr thread = lock_and_find(thid, host.kernel.threads, host.kernel.mutex);

    run_on_current(*thread, entry_point, args, argp);
    return modId;
}

EXPORT(int, sceKernelLockLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return lock_mutex(host, thread_id, host.kernel.lwmutexes, lwmutexid, lock_count, ptimeout, true);
}

EXPORT(int, sceKernelLockLwMutexCB) {
    return unimplemented("sceKernelLockLwMutexCB");
}

EXPORT(int, sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout) {
    return lock_mutex(host, thread_id, host.kernel.mutexes, mutexid, lock_count, timeout, false);
}

EXPORT(int, sceKernelLockMutexCB) {
    return unimplemented("sceKernelLockMutexCB");
}

EXPORT(int, sceKernelLockReadRWLock) {
    return unimplemented("sceKernelLockReadRWLock");
}

EXPORT(int, sceKernelLockReadRWLockCB) {
    return unimplemented("sceKernelLockReadRWLockCB");
}

EXPORT(int, sceKernelLockWriteRWLock) {
    return unimplemented("sceKernelLockWriteRWLock");
}

EXPORT(int, sceKernelLockWriteRWLockCB) {
    return unimplemented("sceKernelLockWriteRWLockCB");
}

EXPORT(int, sceKernelOpenModule) {
    return unimplemented("sceKernelOpenModule");
}

EXPORT(int, sceKernelPMonThreadGetCounter) {
    return unimplemented("sceKernelPMonThreadGetCounter");
}

EXPORT(int, sceKernelPollEvent) {
    return unimplemented("sceKernelPollEvent");
}

EXPORT(int, sceKernelPollEventFlag) {
    return unimplemented("sceKernelPollEventFlag");
}

EXPORT(int, sceKernelPrintBacktrace) {
    return unimplemented("sceKernelPrintBacktrace");
}

EXPORT(int, sceKernelPulseEventWithNotifyCallback) {
    return unimplemented("sceKernelPulseEventWithNotifyCallback");
}

EXPORT(int, sceKernelReceiveMsgPipe) {
    return unimplemented("sceKernelReceiveMsgPipe");
}

EXPORT(int, sceKernelReceiveMsgPipeCB) {
    return unimplemented("sceKernelReceiveMsgPipeCB");
}

EXPORT(int, sceKernelReceiveMsgPipeVector) {
    return unimplemented("sceKernelReceiveMsgPipeVector");
}

EXPORT(int, sceKernelReceiveMsgPipeVectorCB) {
    return unimplemented("sceKernelReceiveMsgPipeVectorCB");
}

EXPORT(int, sceKernelRegisterThreadEventHandler) {
    return unimplemented("sceKernelRegisterThreadEventHandler");
}

EXPORT(int, sceKernelSendMsgPipe) {
    return unimplemented("sceKernelSendMsgPipe");
}

EXPORT(int, sceKernelSendMsgPipeCB) {
    return unimplemented("sceKernelSendMsgPipeCB");
}

EXPORT(int, sceKernelSendMsgPipeVector) {
    return unimplemented("sceKernelSendMsgPipeVector");
}

EXPORT(int, sceKernelSendMsgPipeVectorCB) {
    return unimplemented("sceKernelSendMsgPipeVectorCB");
}

EXPORT(int, sceKernelSetEventWithNotifyCallback) {
    return unimplemented("sceKernelSetEventWithNotifyCallback");
}

EXPORT(int, sceKernelSetThreadContextForVM) {
    return unimplemented("sceKernelSetThreadContextForVM");
}

EXPORT(int, sceKernelSetTimerEvent) {
    return unimplemented("sceKernelSetTimerEvent");
}

EXPORT(int, sceKernelSetTimerTime) {
    return unimplemented("sceKernelSetTimerTime");
}

EXPORT(int, sceKernelSignalLwCond) {
    return unimplemented("sceKernelSignalLwCond");
}

EXPORT(int, sceKernelSignalLwCondAll) {
    return unimplemented("sceKernelSignalLwCondAll");
}

EXPORT(int, sceKernelSignalLwCondTo) {
    return unimplemented("sceKernelSignalLwCondTo");
}

EXPORT(int, sceKernelStackChkFail) {
    return unimplemented("sceKernelStackChkFail");
}

EXPORT(int, sceKernelStartModule) {
    return unimplemented("sceKernelStartModule");
}

EXPORT(int, sceKernelStartThread, SceUID thid, SceSize arglen, Ptr<void> argp) {
    Ptr<void> new_argp = copy_stack(thid, thread_id, argp, host.kernel, host.mem);
    const int res = start_thread(host.kernel, thid, arglen, new_argp);
    if (res < 0) {
        return RET_ERROR("sceKernelStartThread", res);
    }
    return res;
}

EXPORT(int, sceKernelStopModule) {
    return unimplemented("sceKernelStopModule");
}

EXPORT(int, sceKernelStopUnloadModule) {
    return unimplemented("sceKernelStopUnloadModule");
}

EXPORT(int, sceKernelTryLockLwMutex) {
    return unimplemented("sceKernelTryLockLwMutex");
}

EXPORT(int, sceKernelTryReceiveMsgPipe) {
    return unimplemented("sceKernelTryReceiveMsgPipe");
}

EXPORT(int, sceKernelTryReceiveMsgPipeVector) {
    return unimplemented("sceKernelTryReceiveMsgPipeVector");
}

EXPORT(int, sceKernelTrySendMsgPipe) {
    return unimplemented("sceKernelTrySendMsgPipe");
}

EXPORT(int, sceKernelTrySendMsgPipeVector) {
    return unimplemented("sceKernelTrySendMsgPipeVector");
}

EXPORT(int, sceKernelUnloadModule) {
    return unimplemented("sceKernelUnloadModule");
}

EXPORT(int, sceKernelUnlockLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea, int unlock_count) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return unlock_mutex(host, thread_id, host.kernel.lwmutexes, lwmutexid, unlock_count);
}

EXPORT(int, sceKernelUnlockLwMutex2, Ptr<emu::SceKernelLwMutexWork> workarea, int unlock_count) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return unlock_mutex(host, thread_id, host.kernel.lwmutexes, lwmutexid, unlock_count);
}

EXPORT(int, sceKernelWaitCond) {
    return unimplemented("sceKernelWaitCond");
}

EXPORT(int, sceKernelWaitCondCB) {
    return unimplemented("sceKernelWaitCondCB");
}

EXPORT(int, sceKernelWaitEvent) {
    return unimplemented("sceKernelWaitEvent");
}

EXPORT(int, sceKernelWaitEventCB) {
    return unimplemented("sceKernelWaitEventCB");
}

EXPORT(int, sceKernelWaitEventFlag) {
    return unimplemented("sceKernelWaitEventFlag");
}

EXPORT(int, sceKernelWaitEventFlagCB) {
    return unimplemented("sceKernelWaitEventFlagCB");
}

EXPORT(int, sceKernelWaitException) {
    return unimplemented("sceKernelWaitException");
}

EXPORT(int, sceKernelWaitExceptionCB) {
    return unimplemented("sceKernelWaitExceptionCB");
}

EXPORT(int, sceKernelWaitLwCond) {
    return unimplemented("sceKernelWaitLwCond");
}

EXPORT(int, sceKernelWaitLwCondCB) {
    return unimplemented("sceKernelWaitLwCondCB");
}

EXPORT(int, sceKernelWaitMultipleEvents) {
    return unimplemented("sceKernelWaitMultipleEvents");
}

EXPORT(int, sceKernelWaitMultipleEventsCB) {
    return unimplemented("sceKernelWaitMultipleEventsCB");
}

EXPORT(int, sceKernelWaitSema, SceUID semaid, int signal, SceUInt *timeout) {
    return wait_semaphore(host, thread_id, semaid, signal, timeout);
}

EXPORT(int, sceKernelWaitSemaCB) {
    return unimplemented("sceKernelWaitSemaCB");
}

EXPORT(int, sceKernelWaitSignal) {
    return unimplemented("sceKernelWaitSignal");
}

EXPORT(int, sceKernelWaitSignalCB) {
    return unimplemented("sceKernelWaitSignalCB");
}

EXPORT(int, sceKernelWaitThreadEnd, SceUID thid, int *stat, SceUInt *timeout) {
    const ThreadStatePtr cur_thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    {
        const std::lock_guard<std::mutex> lock(cur_thread->mutex);
        assert(cur_thread->to_do == ThreadToDo::run);
        cur_thread->to_do = ThreadToDo::wait;
        stop(*cur_thread->cpu);
    }

    {
        const ThreadStatePtr thread = lock_and_find(thid, host.kernel.threads, host.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        thread->waiting_threads.push_back(cur_thread);
    }

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelWaitThreadEndCB) {
    return unimplemented("sceKernelWaitThreadEndCB");
}

EXPORT(int, sceSblACMgrIsGameProgram) {
    return unimplemented("sceSblACMgrIsGameProgram");
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Shutdown) {
    return unimplemented("sceSblGcAuthMgrAdhocBB160Shutdown");
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Shutdown) {
    return unimplemented("sceSblGcAuthMgrAdhocBB224Shutdown");
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBCipherFinal) {
    return unimplemented("sceSblGcAuthMgrMsSaveBBCipherFinal");
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBMacUpdate) {
    return unimplemented("sceSblGcAuthMgrMsSaveBBMacUpdate");
}

EXPORT(int, sceSblGcAuthMgrPcactActivation) {
    return unimplemented("sceSblGcAuthMgrPcactActivation");
}

BRIDGE_IMPL(SceKernelStackChkGuard)
BRIDGE_IMPL(__sce_aeabi_idiv0)
BRIDGE_IMPL(__sce_aeabi_ldiv0)
BRIDGE_IMPL(__stack_chk_fail)
BRIDGE_IMPL(__stack_chk_guard)
BRIDGE_IMPL(_sceKernelCreateLwMutex)
BRIDGE_IMPL(sceClibAbort)
BRIDGE_IMPL(sceClibDprintf)
BRIDGE_IMPL(sceClibLookCtypeTable)
BRIDGE_IMPL(sceClibMemchr)
BRIDGE_IMPL(sceClibMemcmp)
BRIDGE_IMPL(sceClibMemcmpConstTime)
BRIDGE_IMPL(sceClibMemcpy)
BRIDGE_IMPL(sceClibMemcpyChk)
BRIDGE_IMPL(sceClibMemcpy_safe)
BRIDGE_IMPL(sceClibMemmove)
BRIDGE_IMPL(sceClibMemmoveChk)
BRIDGE_IMPL(sceClibMemset)
BRIDGE_IMPL(sceClibMemsetChk)
BRIDGE_IMPL(sceClibMspaceCalloc)
BRIDGE_IMPL(sceClibMspaceCreate)
BRIDGE_IMPL(sceClibMspaceDestroy)
BRIDGE_IMPL(sceClibMspaceFree)
BRIDGE_IMPL(sceClibMspaceIsHeapEmpty)
BRIDGE_IMPL(sceClibMspaceMalloc)
BRIDGE_IMPL(sceClibMspaceMallocStats)
BRIDGE_IMPL(sceClibMspaceMallocStatsFast)
BRIDGE_IMPL(sceClibMspaceMallocUsableSize)
BRIDGE_IMPL(sceClibMspaceMemalign)
BRIDGE_IMPL(sceClibMspaceRealloc)
BRIDGE_IMPL(sceClibMspaceReallocalign)
BRIDGE_IMPL(sceClibPrintf)
BRIDGE_IMPL(sceClibSnprintf)
BRIDGE_IMPL(sceClibSnprintfChk)
BRIDGE_IMPL(sceClibStrcatChk)
BRIDGE_IMPL(sceClibStrchr)
BRIDGE_IMPL(sceClibStrcmp)
BRIDGE_IMPL(sceClibStrcpyChk)
BRIDGE_IMPL(sceClibStrlcat)
BRIDGE_IMPL(sceClibStrlcatChk)
BRIDGE_IMPL(sceClibStrlcpy)
BRIDGE_IMPL(sceClibStrlcpyChk)
BRIDGE_IMPL(sceClibStrncasecmp)
BRIDGE_IMPL(sceClibStrncat)
BRIDGE_IMPL(sceClibStrncatChk)
BRIDGE_IMPL(sceClibStrncmp)
BRIDGE_IMPL(sceClibStrncpy)
BRIDGE_IMPL(sceClibStrncpyChk)
BRIDGE_IMPL(sceClibStrnlen)
BRIDGE_IMPL(sceClibStrrchr)
BRIDGE_IMPL(sceClibStrstr)
BRIDGE_IMPL(sceClibStrtoll)
BRIDGE_IMPL(sceClibTolower)
BRIDGE_IMPL(sceClibToupper)
BRIDGE_IMPL(sceClibVdprintf)
BRIDGE_IMPL(sceClibVprintf)
BRIDGE_IMPL(sceClibVsnprintf)
BRIDGE_IMPL(sceClibVsnprintfChk)
BRIDGE_IMPL(sceIoChstat)
BRIDGE_IMPL(sceIoChstatAsync)
BRIDGE_IMPL(sceIoChstatByFd)
BRIDGE_IMPL(sceIoCompleteMultiple)
BRIDGE_IMPL(sceIoDevctl)
BRIDGE_IMPL(sceIoDevctlAsync)
BRIDGE_IMPL(sceIoDopen)
BRIDGE_IMPL(sceIoDread)
BRIDGE_IMPL(sceIoGetstat)
BRIDGE_IMPL(sceIoGetstatAsync)
BRIDGE_IMPL(sceIoGetstatByFd)
BRIDGE_IMPL(sceIoIoctl)
BRIDGE_IMPL(sceIoIoctlAsync)
BRIDGE_IMPL(sceIoLseek)
BRIDGE_IMPL(sceIoLseekAsync)
BRIDGE_IMPL(sceIoMkdir)
BRIDGE_IMPL(sceIoMkdirAsync)
BRIDGE_IMPL(sceIoOpen)
BRIDGE_IMPL(sceIoOpenAsync)
BRIDGE_IMPL(sceIoPread)
BRIDGE_IMPL(sceIoPreadAsync)
BRIDGE_IMPL(sceIoPwrite)
BRIDGE_IMPL(sceIoPwriteAsync)
BRIDGE_IMPL(sceIoRemove)
BRIDGE_IMPL(sceIoRemoveAsync)
BRIDGE_IMPL(sceIoRename)
BRIDGE_IMPL(sceIoRenameAsync)
BRIDGE_IMPL(sceIoRmdir)
BRIDGE_IMPL(sceIoRmdirAsync)
BRIDGE_IMPL(sceIoSync)
BRIDGE_IMPL(sceIoSyncAsync)
BRIDGE_IMPL(sceKernelAtomicAddAndGet16)
BRIDGE_IMPL(sceKernelAtomicAddAndGet32)
BRIDGE_IMPL(sceKernelAtomicAddAndGet64)
BRIDGE_IMPL(sceKernelAtomicAddAndGet8)
BRIDGE_IMPL(sceKernelAtomicAddUnless16)
BRIDGE_IMPL(sceKernelAtomicAddUnless32)
BRIDGE_IMPL(sceKernelAtomicAddUnless64)
BRIDGE_IMPL(sceKernelAtomicAddUnless8)
BRIDGE_IMPL(sceKernelAtomicAndAndGet16)
BRIDGE_IMPL(sceKernelAtomicAndAndGet32)
BRIDGE_IMPL(sceKernelAtomicAndAndGet64)
BRIDGE_IMPL(sceKernelAtomicAndAndGet8)
BRIDGE_IMPL(sceKernelAtomicClearAndGet16)
BRIDGE_IMPL(sceKernelAtomicClearAndGet32)
BRIDGE_IMPL(sceKernelAtomicClearAndGet64)
BRIDGE_IMPL(sceKernelAtomicClearAndGet8)
BRIDGE_IMPL(sceKernelAtomicClearMask16)
BRIDGE_IMPL(sceKernelAtomicClearMask32)
BRIDGE_IMPL(sceKernelAtomicClearMask64)
BRIDGE_IMPL(sceKernelAtomicClearMask8)
BRIDGE_IMPL(sceKernelAtomicCompareAndSet16)
BRIDGE_IMPL(sceKernelAtomicCompareAndSet32)
BRIDGE_IMPL(sceKernelAtomicCompareAndSet64)
BRIDGE_IMPL(sceKernelAtomicCompareAndSet8)
BRIDGE_IMPL(sceKernelAtomicDecIfPositive16)
BRIDGE_IMPL(sceKernelAtomicDecIfPositive32)
BRIDGE_IMPL(sceKernelAtomicDecIfPositive64)
BRIDGE_IMPL(sceKernelAtomicDecIfPositive8)
BRIDGE_IMPL(sceKernelAtomicGetAndAdd16)
BRIDGE_IMPL(sceKernelAtomicGetAndAdd32)
BRIDGE_IMPL(sceKernelAtomicGetAndAdd64)
BRIDGE_IMPL(sceKernelAtomicGetAndAdd8)
BRIDGE_IMPL(sceKernelAtomicGetAndAnd16)
BRIDGE_IMPL(sceKernelAtomicGetAndAnd32)
BRIDGE_IMPL(sceKernelAtomicGetAndAnd64)
BRIDGE_IMPL(sceKernelAtomicGetAndAnd8)
BRIDGE_IMPL(sceKernelAtomicGetAndClear16)
BRIDGE_IMPL(sceKernelAtomicGetAndClear32)
BRIDGE_IMPL(sceKernelAtomicGetAndClear64)
BRIDGE_IMPL(sceKernelAtomicGetAndClear8)
BRIDGE_IMPL(sceKernelAtomicGetAndOr16)
BRIDGE_IMPL(sceKernelAtomicGetAndOr32)
BRIDGE_IMPL(sceKernelAtomicGetAndOr64)
BRIDGE_IMPL(sceKernelAtomicGetAndOr8)
BRIDGE_IMPL(sceKernelAtomicGetAndSet16)
BRIDGE_IMPL(sceKernelAtomicGetAndSet32)
BRIDGE_IMPL(sceKernelAtomicGetAndSet64)
BRIDGE_IMPL(sceKernelAtomicGetAndSet8)
BRIDGE_IMPL(sceKernelAtomicGetAndSub16)
BRIDGE_IMPL(sceKernelAtomicGetAndSub32)
BRIDGE_IMPL(sceKernelAtomicGetAndSub64)
BRIDGE_IMPL(sceKernelAtomicGetAndSub8)
BRIDGE_IMPL(sceKernelAtomicGetAndXor16)
BRIDGE_IMPL(sceKernelAtomicGetAndXor32)
BRIDGE_IMPL(sceKernelAtomicGetAndXor64)
BRIDGE_IMPL(sceKernelAtomicGetAndXor8)
BRIDGE_IMPL(sceKernelAtomicOrAndGet16)
BRIDGE_IMPL(sceKernelAtomicOrAndGet32)
BRIDGE_IMPL(sceKernelAtomicOrAndGet64)
BRIDGE_IMPL(sceKernelAtomicOrAndGet8)
BRIDGE_IMPL(sceKernelAtomicSet16)
BRIDGE_IMPL(sceKernelAtomicSet32)
BRIDGE_IMPL(sceKernelAtomicSet64)
BRIDGE_IMPL(sceKernelAtomicSet8)
BRIDGE_IMPL(sceKernelAtomicSubAndGet16)
BRIDGE_IMPL(sceKernelAtomicSubAndGet32)
BRIDGE_IMPL(sceKernelAtomicSubAndGet64)
BRIDGE_IMPL(sceKernelAtomicSubAndGet8)
BRIDGE_IMPL(sceKernelAtomicXorAndGet16)
BRIDGE_IMPL(sceKernelAtomicXorAndGet32)
BRIDGE_IMPL(sceKernelAtomicXorAndGet64)
BRIDGE_IMPL(sceKernelAtomicXorAndGet8)
BRIDGE_IMPL(sceKernelBacktrace)
BRIDGE_IMPL(sceKernelBacktraceSelf)
BRIDGE_IMPL(sceKernelCallModuleExit)
BRIDGE_IMPL(sceKernelCallWithChangeStack)
BRIDGE_IMPL(sceKernelCancelEvent)
BRIDGE_IMPL(sceKernelCancelEventFlag)
BRIDGE_IMPL(sceKernelCancelEventWithSetPattern)
BRIDGE_IMPL(sceKernelCancelMsgPipe)
BRIDGE_IMPL(sceKernelCancelMutex)
BRIDGE_IMPL(sceKernelCancelRWLock)
BRIDGE_IMPL(sceKernelCancelSema)
BRIDGE_IMPL(sceKernelCancelTimer)
BRIDGE_IMPL(sceKernelChangeCurrentThreadAttr)
BRIDGE_IMPL(sceKernelCheckThreadStack)
BRIDGE_IMPL(sceKernelCloseModule)
BRIDGE_IMPL(sceKernelCreateCond)
BRIDGE_IMPL(sceKernelCreateEventFlag)
BRIDGE_IMPL(sceKernelCreateLwCond)
BRIDGE_IMPL(sceKernelCreateLwMutex)
BRIDGE_IMPL(sceKernelCreateMsgPipe)
BRIDGE_IMPL(sceKernelCreateMsgPipeWithLR)
BRIDGE_IMPL(sceKernelCreateMutex)
BRIDGE_IMPL(sceKernelCreateRWLock)
BRIDGE_IMPL(sceKernelCreateSema)
BRIDGE_IMPL(sceKernelCreateSema_16XX)
BRIDGE_IMPL(sceKernelCreateSimpleEvent)
BRIDGE_IMPL(sceKernelCreateThread)
BRIDGE_IMPL(sceKernelCreateTimer)
BRIDGE_IMPL(sceKernelDeleteLwCond)
BRIDGE_IMPL(sceKernelDeleteLwMutex)
BRIDGE_IMPL(sceKernelExitProcess)
BRIDGE_IMPL(sceKernelGetCallbackInfo)
BRIDGE_IMPL(sceKernelGetCondInfo)
BRIDGE_IMPL(sceKernelGetCurrentThreadVfpException)
BRIDGE_IMPL(sceKernelGetEventFlagInfo)
BRIDGE_IMPL(sceKernelGetEventInfo)
BRIDGE_IMPL(sceKernelGetEventPattern)
BRIDGE_IMPL(sceKernelGetLwCondInfo)
BRIDGE_IMPL(sceKernelGetLwCondInfoById)
BRIDGE_IMPL(sceKernelGetLwMutexInfo)
BRIDGE_IMPL(sceKernelGetLwMutexInfoById)
BRIDGE_IMPL(sceKernelGetModuleInfoByAddr)
BRIDGE_IMPL(sceKernelGetMsgPipeInfo)
BRIDGE_IMPL(sceKernelGetMutexInfo)
BRIDGE_IMPL(sceKernelGetOpenPsId)
BRIDGE_IMPL(sceKernelGetPMUSERENR)
BRIDGE_IMPL(sceKernelGetProcessTime)
BRIDGE_IMPL(sceKernelGetProcessTimeLow)
BRIDGE_IMPL(sceKernelGetProcessTimeWide)
BRIDGE_IMPL(sceKernelGetRWLockInfo)
BRIDGE_IMPL(sceKernelGetSemaInfo)
BRIDGE_IMPL(sceKernelGetSystemInfo)
BRIDGE_IMPL(sceKernelGetSystemTime)
BRIDGE_IMPL(sceKernelGetTLSAddr)
BRIDGE_IMPL(sceKernelGetThreadContextForVM)
BRIDGE_IMPL(sceKernelGetThreadCpuAffinityMask)
BRIDGE_IMPL(sceKernelGetThreadCurrentPriority)
BRIDGE_IMPL(sceKernelGetThreadEventInfo)
BRIDGE_IMPL(sceKernelGetThreadExitStatus)
BRIDGE_IMPL(sceKernelGetThreadId)
BRIDGE_IMPL(sceKernelGetThreadInfo)
BRIDGE_IMPL(sceKernelGetThreadRunStatus)
BRIDGE_IMPL(sceKernelGetTimerBase)
BRIDGE_IMPL(sceKernelGetTimerEventRemainingTime)
BRIDGE_IMPL(sceKernelGetTimerInfo)
BRIDGE_IMPL(sceKernelGetTimerTime)
BRIDGE_IMPL(sceKernelLoadModule)
BRIDGE_IMPL(sceKernelLoadStartModule)
BRIDGE_IMPL(sceKernelLockLwMutex)
BRIDGE_IMPL(sceKernelLockLwMutexCB)
BRIDGE_IMPL(sceKernelLockMutex)
BRIDGE_IMPL(sceKernelLockMutexCB)
BRIDGE_IMPL(sceKernelLockReadRWLock)
BRIDGE_IMPL(sceKernelLockReadRWLockCB)
BRIDGE_IMPL(sceKernelLockWriteRWLock)
BRIDGE_IMPL(sceKernelLockWriteRWLockCB)
BRIDGE_IMPL(sceKernelOpenModule)
BRIDGE_IMPL(sceKernelPMonThreadGetCounter)
BRIDGE_IMPL(sceKernelPollEvent)
BRIDGE_IMPL(sceKernelPollEventFlag)
BRIDGE_IMPL(sceKernelPrintBacktrace)
BRIDGE_IMPL(sceKernelPulseEventWithNotifyCallback)
BRIDGE_IMPL(sceKernelReceiveMsgPipe)
BRIDGE_IMPL(sceKernelReceiveMsgPipeCB)
BRIDGE_IMPL(sceKernelReceiveMsgPipeVector)
BRIDGE_IMPL(sceKernelReceiveMsgPipeVectorCB)
BRIDGE_IMPL(sceKernelRegisterThreadEventHandler)
BRIDGE_IMPL(sceKernelSendMsgPipe)
BRIDGE_IMPL(sceKernelSendMsgPipeCB)
BRIDGE_IMPL(sceKernelSendMsgPipeVector)
BRIDGE_IMPL(sceKernelSendMsgPipeVectorCB)
BRIDGE_IMPL(sceKernelSetEventWithNotifyCallback)
BRIDGE_IMPL(sceKernelSetThreadContextForVM)
BRIDGE_IMPL(sceKernelSetTimerEvent)
BRIDGE_IMPL(sceKernelSetTimerTime)
BRIDGE_IMPL(sceKernelSignalLwCond)
BRIDGE_IMPL(sceKernelSignalLwCondAll)
BRIDGE_IMPL(sceKernelSignalLwCondTo)
BRIDGE_IMPL(sceKernelStackChkFail)
BRIDGE_IMPL(sceKernelStartModule)
BRIDGE_IMPL(sceKernelStartThread)
BRIDGE_IMPL(sceKernelStopModule)
BRIDGE_IMPL(sceKernelStopUnloadModule)
BRIDGE_IMPL(sceKernelTryLockLwMutex)
BRIDGE_IMPL(sceKernelTryReceiveMsgPipe)
BRIDGE_IMPL(sceKernelTryReceiveMsgPipeVector)
BRIDGE_IMPL(sceKernelTrySendMsgPipe)
BRIDGE_IMPL(sceKernelTrySendMsgPipeVector)
BRIDGE_IMPL(sceKernelUnloadModule)
BRIDGE_IMPL(sceKernelUnlockLwMutex)
BRIDGE_IMPL(sceKernelUnlockLwMutex2)
BRIDGE_IMPL(sceKernelWaitCond)
BRIDGE_IMPL(sceKernelWaitCondCB)
BRIDGE_IMPL(sceKernelWaitEvent)
BRIDGE_IMPL(sceKernelWaitEventCB)
BRIDGE_IMPL(sceKernelWaitEventFlag)
BRIDGE_IMPL(sceKernelWaitEventFlagCB)
BRIDGE_IMPL(sceKernelWaitException)
BRIDGE_IMPL(sceKernelWaitExceptionCB)
BRIDGE_IMPL(sceKernelWaitLwCond)
BRIDGE_IMPL(sceKernelWaitLwCondCB)
BRIDGE_IMPL(sceKernelWaitMultipleEvents)
BRIDGE_IMPL(sceKernelWaitMultipleEventsCB)
BRIDGE_IMPL(sceKernelWaitSema)
BRIDGE_IMPL(sceKernelWaitSemaCB)
BRIDGE_IMPL(sceKernelWaitSignal)
BRIDGE_IMPL(sceKernelWaitSignalCB)
BRIDGE_IMPL(sceKernelWaitThreadEnd)
BRIDGE_IMPL(sceKernelWaitThreadEndCB)
BRIDGE_IMPL(sceSblACMgrIsGameProgram)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160Shutdown)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224Shutdown)
BRIDGE_IMPL(sceSblGcAuthMgrMsSaveBBCipherFinal)
BRIDGE_IMPL(sceSblGcAuthMgrMsSaveBBMacUpdate)
BRIDGE_IMPL(sceSblGcAuthMgrPcactActivation)
