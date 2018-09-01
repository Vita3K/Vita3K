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
#include <dlmalloc.h>
#include <host/functions.h>
#include <io/functions.h>
#include <io/types.h>
#include <kernel/functions.h>
#include <kernel/thread/sync_primitives.h>
#include <kernel/thread/thread_functions.h>
#include <kernel/types.h>
#include <rtc/rtc.h>
#include <util/lock_and_find.h>
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

EXPORT(int, SceKernelStackChkGuard) {
    return UNIMPLEMENTED();
}

EXPORT(int, __sce_aeabi_idiv0) {
    return UNIMPLEMENTED();
}

EXPORT(int, __sce_aeabi_ldiv0) {
    return UNIMPLEMENTED();
}

EXPORT(int, __stack_chk_fail) {
    return UNIMPLEMENTED();
}

EXPORT(int, __stack_chk_guard) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, int init_count, const SceKernelLwMutexOptParam *opt_param) {
    assert(name != nullptr);
    assert(init_count >= 0);

    auto uid_out = &workarea.get(host.mem)->uid;
    return mutex_create(uid_out, host.kernel, export_name, thread_id, name, attr, init_count, SyncWeight::Light);
}

EXPORT(int, sceClibAbort) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibDprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibLookCtypeTable) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMemchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMemcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMemcmpConstTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMemcpy, void *destination, const void *source, SceSize num) {
    memcpy(destination, source, num);
    return 0;
}

EXPORT(int, sceClibMemcpyChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMemcpy_safe, void *destination, const void *source, SceSize num) {
    memcpy(destination, source, num);
    return 0;
}

EXPORT(int, sceClibMemmove) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMemmoveChk) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMemset, Ptr<void> s, int c, SceSize n) {
    memset(s.get(host.mem), c, n);
    return s;
}

EXPORT(int, sceClibMemsetChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceCalloc) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMspaceCreate, Ptr<void> base, uint32_t capacity) {
    mspace space = create_mspace_with_base(base.get(host.mem), capacity, 0);
    return Ptr<void>(space, host.mem);
}

EXPORT(int, sceClibMspaceDestroy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceFree) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceIsHeapEmpty) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMspaceMalloc, Ptr<void> space, uint32_t size) {
    void *address = mspace_malloc(space.get(host.mem), size);
    return Ptr<void>(address, host.mem);
}

EXPORT(int, sceClibMspaceMallocStats) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceMallocStatsFast) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceMallocUsableSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceMemalign) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceRealloc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceReallocalign) {
    return UNIMPLEMENTED();
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
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrcatChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrchr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrcmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrcpyChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrlcat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrlcatChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrlcpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrlcpyChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncasecmp, const char *string1, const char *string2, int count) {
#ifdef WIN32
    return strnicmp(string1, string2, count);
#else
    return strncasecmp(string1, string2, count);
#endif
}

EXPORT(int, sceClibStrncat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncatChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncmp) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncpy) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncpyChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrnlen) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, sceClibStrrchr, const char *str, int character) {
    const char *res = strrchr(str, character);
    return Ptr<char>(res, host.mem);
}

EXPORT(int, sceClibStrstr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrtoll) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibTolower) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibToupper) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibVdprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibVprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibVsnprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibVsnprintfChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoChstat) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoChstatAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoChstatByFd) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoCompleteMultiple) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDevctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDopen, const char *dir) {
    return open_dir(host.io, dir, host.pref_path.c_str(), export_name);
}

EXPORT(int, sceIoDevctlAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDread, SceUID fd, emu::SceIoDirent *dir) {
    if (dir == nullptr) {
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    }
    return read_dir(host.io, fd, dir, export_name);
}

EXPORT(int, sceIoGetstat, const char *file, SceIoStat *stat) {
    return stat_file(host.io, file, stat, host.pref_path.c_str(), host.kernel.base_tick.tick, export_name);
}

EXPORT(int, sceIoGetstatAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetstatByFd, const int fd, SceIoStat *stat) {
    return stat_file_by_fd(host.io, fd, stat, host.pref_path.c_str(), host.kernel.base_tick.tick, export_name);
}

EXPORT(int, sceIoIoctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoIoctlAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoLseek, SceUID fd, SceOff offset, int whence) {
    return seek_file(fd, offset, whence, host.io, export_name);
}

EXPORT(int, sceIoMkdir, const char *dir, SceMode mode) {
    return create_dir(host.io, dir, mode, host.pref_path.c_str(), export_name);
}

EXPORT(int, sceIoMkdirAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoLseekAsync) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceIoOpen, const char *file, int flags, SceMode mode) {
    if (file == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    LOG_INFO("Opening file: {}", file);
    return open_file(host.io, file, flags, host.pref_path.c_str(), export_name);
}

EXPORT(int, sceIoOpenAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoPread, SceUID fd, void *data, SceSize size, SceOff offset) {
    if (seek_file(fd, offset, SEEK_SET, host.io, export_name) < 0) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    return read_file(data, host.io, fd, size, export_name);
}

EXPORT(int, sceIoPreadAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoPwrite, SceUID fd, const void *data, SceSize size, SceOff offset) {
    if (seek_file(fd, offset, SEEK_SET, host.io, export_name) < 0) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    return write_file(fd, data, size, host.io, export_name);
}

EXPORT(int, sceIoPwriteAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRemove, const char *path) {
    if (path == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    return remove_file(host.io, path, host.pref_path.c_str(), export_name);
}

EXPORT(int, sceIoRemoveAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRename) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRenameAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRmdir, const char *path) {
    if (path == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    return remove_dir(host.io, path, host.pref_path.c_str(), export_name);
}

EXPORT(int, sceIoRmdirAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSyncAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddAndGet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddAndGet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddAndGet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddAndGet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddUnless16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddUnless32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddUnless64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddUnless8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAndAndGet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAndAndGet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAndAndGet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAndAndGet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearAndGet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearAndGet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearAndGet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearAndGet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearMask16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearMask32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearMask64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearMask8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicCompareAndSet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicCompareAndSet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicCompareAndSet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicCompareAndSet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicDecIfPositive16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicDecIfPositive32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicDecIfPositive64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicDecIfPositive8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAdd16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAdd32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAdd64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAdd8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAnd16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAnd32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAnd64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAnd8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndClear16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndClear32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndClear64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndClear8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndOr16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndOr32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndOr64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndOr8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSub16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSub32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSub64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSub8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndXor16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndXor32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndXor64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndXor8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicOrAndGet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicOrAndGet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicOrAndGet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicOrAndGet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSubAndGet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSubAndGet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSubAndGet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSubAndGet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicXorAndGet16) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicXorAndGet32) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicXorAndGet64) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicXorAndGet8) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelBacktrace) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelBacktraceSelf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCallModuleExit) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCallWithChangeStack) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelEventWithSetPattern) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelSema) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelChangeCurrentThreadAttr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCheckThreadStack) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCreateCond, const char *name, SceUInt attr, SceUID mutexid, void *opt_param) {
    SceUID uid;

    if (auto error = condvar_create(&uid, host.kernel, export_name, name, attr, mutexid, SyncWeight::Heavy)) {
        return error;
    }
    return uid;
}

EXPORT(int, sceKernelCreateEventFlag, const char *name, unsigned int attr, unsigned int flags, SceKernelEventFlagOptParam *opt) {
    return eventflag_create(host.kernel, export_name, thread_id, name, attr, flags);
}

EXPORT(int, sceKernelCreateLwCond, Ptr<emu::SceKernelLwCondWork> workarea, const char *name, SceUInt attr, Ptr<emu::SceKernelLwMutexWork> workarea_mutex, const SceKernelLwCondOptParam *opt_param) {
    const auto uid_out = &workarea.get(host.mem)->uid;
    const auto assoc_mutex_uid = workarea_mutex.get(host.mem)->uid;

    return condvar_create(uid_out, host.kernel, export_name, name, attr, assoc_mutex_uid, SyncWeight::Light);
}

EXPORT(int, sceKernelCreateLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea, const char *name, SceUInt attr, int init_count, const SceKernelLwMutexOptParam *opt_param) {
    assert(workarea);
    assert(name);
    assert(init_count >= 0);

    const auto uid_out = &workarea.get(host.mem)->uid;
    return mutex_create(uid_out, host.kernel, export_name, thread_id, name, attr, init_count, SyncWeight::Light);
}

EXPORT(int, sceKernelCreateMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCreateMsgPipeWithLR) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCreateMutex, const char *name, SceUInt attr, int init_count, SceKernelMutexOptParam *opt_param) {
    SceUID uid;

    if (auto error = mutex_create(&uid, host.kernel, export_name, thread_id, name, attr, init_count, SyncWeight::Heavy)) {
        return error;
    }
    return uid;
}

EXPORT(int, sceKernelCreateRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelCreateSema, const char *name, SceUInt attr, int initVal, int maxVal, SceKernelSemaOptParam *option) {
    return semaphore_create(host.kernel, export_name, name, attr, initVal, maxVal);
}

EXPORT(int, sceKernelCreateSema_16XX, const char *name, SceUInt attr, int initVal, int maxVal, SceKernelSemaOptParam *option) {
    return semaphore_create(host.kernel, export_name, name, attr, initVal, maxVal);
}

EXPORT(int, sceKernelCreateSimpleEvent) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelCreateThread, const char *name, emu::SceKernelThreadEntry entry, int init_priority, int stack_size, SceUInt attr, int cpu_affinity_mask, const SceKernelThreadOptParam *option) {
    if (cpu_affinity_mask > 0x70000) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_CPU_AFFINITY);
    }
    const CallImport call_import = [&host](CPUState &cpu, uint32_t nid, SceUID thread_id) {
        ::call_import(host, cpu, nid, thread_id);
    };

    const SceUID thid = create_thread(entry.cast<const void>(), host.kernel, host.mem, name, init_priority, stack_size, call_import, false);
    if (thid < 0)
        return RET_ERROR(thid);
    return thid;
}

EXPORT(int, sceKernelCreateTimer) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelDeleteLwCond, Ptr<emu::SceKernelLwCondWork> workarea) {
    SceUID condid = workarea.get(host.mem)->uid;
    return condvar_delete(host.kernel, export_name, thread_id, condid, SyncWeight::Light);
}

EXPORT(int, sceKernelDeleteLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return mutex_delete(host.kernel, export_name, thread_id, lwmutexid, SyncWeight::Light);
}

EXPORT(int, sceKernelExitProcess, int res) {
    // TODO Handle exit code?
    stop_all_threads(host.kernel);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetCallbackInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetCondInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetCurrentThreadVfpException) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetEventFlagInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetEventInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetEventPattern) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLwCondInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLwCondInfoById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLwMutexInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLwMutexInfoById) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetModuleInfoByAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetMsgPipeInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetMutexInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetOpenPsId) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetPMUSERENR) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessTime, SceUInt64 *time) {
    if (time)
        *time = rtc_get_ticks(host.kernel.base_tick.tick);
    return 0;
}

EXPORT(SceUInt32, sceKernelGetProcessTimeLow) {
    return static_cast<SceUInt32>(rtc_get_ticks(host.kernel.base_tick.tick));
}

EXPORT(SceUInt64, sceKernelGetProcessTimeWide) {
    return rtc_get_ticks(host.kernel.base_tick.tick);
}

EXPORT(int, sceKernelGetRWLockInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetSemaInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetSystemInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetSystemTime) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<Ptr<void>>, sceKernelGetTLSAddr, int key) {
    return get_thread_tls_addr(host.kernel, host.mem, thread_id, key);
}

EXPORT(int, sceKernelGetThreadContextForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadCpuAffinityMask) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadCpuAffinityMask_) {
    STUBBED("STUB");
    return 0x01 << 16;
}

EXPORT(int, sceKernelGetThreadCurrentPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadCurrentPriority_) {
    STUBBED("STUB");
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    const std::lock_guard<std::mutex> lock(thread->mutex);
    return thread.get()->priority;
}

EXPORT(int, sceKernelGetThreadEventInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadExitStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadId) {
    return thread_id;
}

EXPORT(int, sceKernelGetThreadInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadRunStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerBase) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerEventRemainingTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerTime) {
    return UNIMPLEMENTED();
}

/**
 * \brief Loads a dynamic module into memory if it wasn't already loaded. If it was, find it and return it. First 3 arguments are outputs.
 * \param mod_id UID of the loaded module object
 * \param entry_point Entry point (module_start) of the loaded module
 * \param module Module info
 * \param host 
 * \param export_name 
 * \param path File name of module file
 * \param error_val Error value on failure
 * \return True on success, false on failure
 */
bool load_module(SceUID &mod_id, Ptr<const void> &entry_point, SceKernelModuleInfoPtr &module, HostState &host, const char *export_name, char *path, int &error_val) {
    const auto &loaded_modules = host.kernel.loaded_modules;

    SceKernelModuleInfoPtrs::const_iterator module_iter = std::find_if(loaded_modules.begin(), loaded_modules.end(), [path](const auto &p) {
        return std::string(p.second->path) == path;
    });

    if (module_iter == loaded_modules.end()) {
        // module is not loaded, load it here

        SceUID file = open_file(host.io, path, SCE_O_RDONLY, host.pref_path.c_str(), export_name);
        if (file < 0) {
            error_val = RET_ERROR(file);
            return false;
        }
        int size = seek_file(file, 0, SCE_SEEK_END, host.io, export_name);
        if (size < 0) {
            error_val = RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
            return false;
        }
        char *data = new char[size];
        if (seek_file(file, 0, SCE_SEEK_SET, host.io, export_name) < 0) {
            error_val = RET_ERROR(size);
            return false;
        }
        if (read_file(data, host.io, file, size, export_name) < 0) {
            error_val = RET_ERROR(size);
            return false;
        }

        mod_id = load_self(entry_point, host.kernel, host.mem, data, path);

        close_file(host.io, file, export_name);
        delete[] data;
        if (mod_id < 0) {
            error_val = RET_ERROR(mod_id);
            return false;
        }

        module_iter = loaded_modules.find(mod_id);
        module = module_iter->second;
    } else {
        // module is already loaded
        module = module_iter->second;

        mod_id = module_iter->first;
        entry_point = module->module_start;
    }
    return true;
}

EXPORT(int, sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option) {
    SceUID mod_id;
    Ptr<const void> entry_point;
    SceKernelModuleInfoPtr module;

    int error_val;
    if (!load_module(mod_id, entry_point, module, host, export_name, path, error_val))
        return error_val;

    return mod_id;
}

EXPORT(int, sceKernelLoadStartModule, char *path, SceSize args, Ptr<void> argp, int flags, SceKernelLMOption *option, int *status) {
    SceUID mod_id;
    Ptr<const void> entry_point;
    SceKernelModuleInfoPtr module;

    int error_val;
    if (!load_module(mod_id, entry_point, module, host, export_name, path, error_val))
        return error_val;

    const CallImport call_import = [&host](CPUState &cpu, uint32_t nid, SceUID thread_id) {
        ::call_import(host, cpu, nid, thread_id);
    };

    const SceUID thid = create_thread(entry_point.cast<const void>(), host.kernel, host.mem, module->module_name, SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_STACK_SIZE_USER_DEFAULT, call_import, false);

    const ThreadStatePtr thread = lock_and_find(thid, host.kernel.threads, host.kernel.mutex);

    uint32_t result = run_on_current(*thread, entry_point, args, argp);
    char *module_name = module->module_name;

    LOG_INFO("Module {} (at \"{}\") module_start returned {}", module_name, module->path, log_hex(result));

    if (status)
        *status = result;

    thread->to_do = ThreadToDo::exit;
    thread->something_to_do.notify_all(); // TODO Should this be notify_one()?
    host.kernel.running_threads.erase(thid);
    host.kernel.threads.erase(thid);

    return mod_id;
}

EXPORT(int, sceKernelLockLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return mutex_lock(host.kernel, export_name, thread_id, lwmutexid, lock_count, ptimeout, SyncWeight::Light);
}

EXPORT(int, sceKernelLockLwMutexCB, Ptr<emu::SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    STUBBED("no CB");
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return mutex_lock(host.kernel, export_name, thread_id, lwmutexid, lock_count, ptimeout, SyncWeight::Light);
}

EXPORT(int, sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout) {
    return mutex_lock(host.kernel, export_name, thread_id, mutexid, lock_count, timeout, SyncWeight::Heavy);
}

EXPORT(int, sceKernelLockMutexCB, SceUID mutexid, int lock_count, unsigned int *timeout) {
    STUBBED("no CB");
    return mutex_lock(host.kernel, export_name, thread_id, mutexid, lock_count, timeout, SyncWeight::Heavy);
}

EXPORT(int, sceKernelLockReadRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelLockReadRWLockCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelLockWriteRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelLockWriteRWLockCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPMonThreadGetCounter) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPollEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPollEventFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPrintBacktrace) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPulseEventWithNotifyCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelReceiveMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelReceiveMsgPipeCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelReceiveMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelReceiveMsgPipeVectorCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelRegisterThreadEventHandler) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSendMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSendMsgPipeCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSendMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSendMsgPipeVectorCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetEventWithNotifyCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetThreadContextForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetTimerEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetTimerTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSignalLwCond, Ptr<emu::SceKernelLwCondWork> workarea) {
    SceUID condid = workarea.get(host.mem)->uid;
    return condvar_signal(host.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Any), SyncWeight::Light);
}

EXPORT(int, sceKernelSignalLwCondAll, Ptr<emu::SceKernelLwCondWork> workarea) {
    SceUID condid = workarea.get(host.mem)->uid;
    return condvar_signal(host.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::All), SyncWeight::Light);
}

EXPORT(int, sceKernelSignalLwCondTo, Ptr<emu::SceKernelLwCondWork> workarea, SceUID thread_target) {
    SceUID condid = workarea.get(host.mem)->uid;
    return condvar_signal(host.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Specific, thread_target), SyncWeight::Light);
}

EXPORT(int, sceKernelStackChkFail) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelStartModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelStartThread, SceUID thid, SceSize arglen, Ptr<void> argp) {
    Ptr<void> new_argp = copy_stack(thid, thread_id, argp, host.kernel, host.mem);
    const int res = start_thread(host.kernel, thid, arglen, new_argp);
    if (res < 0) {
        return RET_ERROR(res);
    }
    return res;
}

EXPORT(int, sceKernelStopModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelStopUnloadModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryLockLwMutex) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryReceiveMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryReceiveMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTrySendMsgPipe) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTrySendMsgPipeVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnloadModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnlockLwMutex, Ptr<emu::SceKernelLwMutexWork> workarea, int unlock_count) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return mutex_unlock(host.kernel, export_name, thread_id, lwmutexid, unlock_count, SyncWeight::Light);
}

EXPORT(int, sceKernelUnlockLwMutex2, Ptr<emu::SceKernelLwMutexWork> workarea, int unlock_count) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return mutex_unlock(host.kernel, export_name, thread_id, lwmutexid, unlock_count, SyncWeight::Light);
}

EXPORT(int, sceKernelWaitCond, SceUID cond_id, SceUInt32 *timeout) {
    return condvar_wait(host.kernel, export_name, thread_id, cond_id, timeout, SyncWeight::Heavy);
}

EXPORT(int, sceKernelWaitCondCB, SceUID cond_id, SceUInt32 *timeout) {
    STUBBED("no CB");
    return condvar_wait(host.kernel, export_name, thread_id, cond_id, timeout, SyncWeight::Heavy);
}

EXPORT(int, sceKernelWaitEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitEventCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitEventFlag, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits, SceUInt *timeout) {
    return eventflag_wait(host.kernel, export_name, thread_id, event_id, flags, wait, timeout);
}

EXPORT(int, sceKernelWaitEventFlagCB, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits, SceUInt *timeout) {
    STUBBED("no CB");
    return eventflag_wait(host.kernel, export_name, thread_id, event_id, flags, wait, timeout);
}

EXPORT(int, sceKernelWaitException) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitExceptionCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitLwCond, Ptr<emu::SceKernelLwCondWork> workarea, SceUInt32 *timeout) {
    const auto cond_id = workarea.get(host.mem)->uid;
    return condvar_wait(host.kernel, export_name, thread_id, cond_id, timeout, SyncWeight::Light);
}

EXPORT(int, sceKernelWaitLwCondCB, Ptr<emu::SceKernelLwCondWork> workarea, SceUInt32 *timeout) {
    STUBBED("no CB");
    const auto cond_id = workarea.get(host.mem)->uid;
    return condvar_wait(host.kernel, export_name, thread_id, cond_id, timeout, SyncWeight::Light);
}

EXPORT(int, sceKernelWaitMultipleEvents) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitMultipleEventsCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitSema, SceUID semaid, int signal, SceUInt *timeout) {
    return semaphore_wait(host.kernel, export_name, thread_id, semaid, signal, timeout);
}

EXPORT(int, sceKernelWaitSemaCB, SceUID semaid, int signal, SceUInt *timeout) {
    STUBBED("no CB");
    return semaphore_wait(host.kernel, export_name, thread_id, semaid, signal, timeout);
}

EXPORT(int, sceKernelWaitSignal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitSignalCB) {
    return UNIMPLEMENTED();
}

int wait_thread_end(HostState &host, SceUID thread_id, SceUID thid) {
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

EXPORT(int, sceKernelWaitThreadEnd, SceUID thid, int *stat, SceUInt *timeout) {
    return wait_thread_end(host, thread_id, thid);
}

EXPORT(int, sceKernelWaitThreadEndCB, SceUID thid, int *stat, SceUInt *timeout) {
    return wait_thread_end(host, thread_id, thid);
}

EXPORT(int, sceSblACMgrIsGameProgram) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Shutdown) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Shutdown) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBCipherFinal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBMacUpdate) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrPcactActivation) {
    return UNIMPLEMENTED();
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
BRIDGE_IMPL(sceKernelGetThreadCpuAffinityMask_)
BRIDGE_IMPL(sceKernelGetThreadCurrentPriority)
BRIDGE_IMPL(sceKernelGetThreadCurrentPriority_)
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
