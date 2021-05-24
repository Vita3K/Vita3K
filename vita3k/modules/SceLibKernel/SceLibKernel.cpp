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
#include <modules/module_parent.h>
#include <v3kprintf.h>

#include <../SceIofilemgr/SceIofilemgr.h>
#include <../SceKernelModulemgr/SceModulemgr.h>
#include <../SceKernelThreadMgr/SceThreadmgr.h>

#include <cpu/functions.h>
#include <dlmalloc.h>
#include <host/functions.h>
#include <host/load_self.h>
#include <io/functions.h>
#include <kernel/sync_primitives.h>

#include <kernel/types.h>
#include <rtc/rtc.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <algorithm>
#include <cstdlib>

enum class TimerFlags : uint32_t {
    FIFO_THREAD = 0x00000000,
    PRIORITY_THREAD = 0x00002000,
    MANUAL_RESET = 0x00000000,
    AUTOMATIC_RESET = 0x00000100,
    ALL_NOTIFY = 0x00000000,
    WAKE_ONLY_NOTIFY = 0x00000800,

    OPENABLE = 0x00000080,
};

inline uint64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
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

EXPORT(int, _sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, int init_count, Ptr<SceKernelLwMutexOptParam> opt_param) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    Ptr<SceKernelCreateLwMutex_opt> options = Ptr<SceKernelCreateLwMutex_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateLwMutex_opt)));
    options.get(host.mem)->init_count = init_count;
    options.get(host.mem)->opt_param = opt_param;
    int res = CALL_EXPORT(__sceKernelCreateLwMutex, workarea, name, attr, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateLwMutex_opt));
    return res;
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

EXPORT(int, sceClibMemcmp, const void *s1, const void *s2, SceSize len) {
    return memcmp(s1, s2, len);
}

EXPORT(int, sceClibMemcmpConstTime) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMemcpy, Ptr<void> dst, const void *src, SceSize len) {
    memcpy(dst.get(host.mem), src, len);
    return dst;
}

EXPORT(int, sceClibMemcpyChk) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMemcpy_safe, Ptr<void> dst, const void *src, SceSize len) {
    memcpy(dst.get(host.mem), src, len);
    return dst;
}

EXPORT(Ptr<void>, sceClibMemmove, Ptr<void> dst, const void *src, SceSize len) {
    memmove(dst.get(host.mem), src, len);
    return dst;
}

EXPORT(int, sceClibMemmoveChk) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMemset, Ptr<void> dst, int ch, SceSize len) {
    memset(dst.get(host.mem), ch, len);
    return dst;
}

EXPORT(int, sceClibMemsetChk) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMspaceCalloc, Ptr<void> space, uint32_t elements, uint32_t size) {
    void *address = mspace_calloc(space.get(host.mem), elements, size);
    return Ptr<void>(address, host.mem);
}

EXPORT(Ptr<void>, sceClibMspaceCreate, Ptr<void> base, uint32_t capacity) {
    mspace space = create_mspace_with_base(base.get(host.mem), capacity, 0);
    return Ptr<void>(space, host.mem);
}

EXPORT(uint32_t, sceClibMspaceDestroy, Ptr<void> space) {
    return destroy_mspace(space.get(host.mem));
}

EXPORT(void, sceClibMspaceFree, Ptr<void> space, Ptr<void> address) {
    mspace_free(space.get(host.mem), address.get(host.mem));
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

EXPORT(Ptr<void>, sceClibMspaceMemalign, Ptr<void> space, uint32_t alignment, uint32_t size) {
    void *address = mspace_memalign(space.get(host.mem), alignment, size);
    return Ptr<void>(address, host.mem);
}

EXPORT(Ptr<void>, sceClibMspaceRealloc, Ptr<void> space, Ptr<void> address, uint32_t size) {
    void *new_address = mspace_realloc(space.get(host.mem), address.get(host.mem), size);
    return Ptr<void>(new_address, host.mem);
}

EXPORT(int, sceClibMspaceReallocalign) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibPrintf, const char *fmt, module::vargs args) {
    std::vector<char> buffer(KB(1));

    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    const int result = utils::snprintf(buffer.data(), buffer.size(), fmt, *(thread->cpu), host.mem, args);

    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    LOG_INFO("{}", buffer.data());

    return SCE_KERNEL_OK;
}

EXPORT(int, sceClibSnprintf, char *dst, SceSize dst_max_size, const char *fmt, module::vargs args) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    int result = utils::snprintf(dst, dst_max_size, fmt, *(thread->cpu), host.mem, args);

    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    return SCE_KERNEL_OK;
}

EXPORT(int, sceClibSnprintfChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrcatChk) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, sceClibStrchr, const char *str, int c) {
    const char *res = strchr(str, c);
    return Ptr<char>(res, host.mem);
}

EXPORT(int, sceClibStrcmp, const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

EXPORT(int, sceClibStrcpyChk) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, sceClibStrlcat, char *dst, const char *src, SceSize len) {
    const char *res = strncat(dst, src, len);
    return Ptr<char>(res, host.mem);
}

EXPORT(int, sceClibStrlcatChk) {
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, sceClibStrlcpy, char *dst, const char *src, SceSize len) {
    const char *res = strncpy(dst, src, len);
    return Ptr<char>(res, host.mem);
}

EXPORT(int, sceClibStrlcpyChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncasecmp, const char *s1, const char *s2, SceSize len) {
#ifdef WIN32
    return _strnicmp(s1, s2, len);
#else
    return strncasecmp(s1, s2, len);
#endif
}

EXPORT(Ptr<char>, sceClibStrncat, char *dst, const char *src, SceSize len) {
    const char *res = strncat(dst, src, len);
    return Ptr<char>(res, host.mem);
}

EXPORT(int, sceClibStrncatChk) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncmp, const char *s1, const char *s2, SceSize len) {
    return strncmp(s1, s2, len);
}

EXPORT(Ptr<char>, sceClibStrncpy, char *dst, const char *src, SceSize len) {
    const char *res = strncpy(dst, src, len);
    return Ptr<char>(res, host.mem);
}

EXPORT(int, sceClibStrncpyChk) {
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceClibStrnlen, const char *s1, SceSize maxlen) {
    return static_cast<uint32_t>(strnlen(s1, maxlen));
}

EXPORT(Ptr<char>, sceClibStrrchr, const char *src, int ch) {
    const char *res = strrchr(src, ch);
    return Ptr<char>(res, host.mem);
}

EXPORT(Ptr<char>, sceClibStrstr, const char *s1, const char *s2) {
    const char *res = strstr(s1, s2);
    return Ptr<char>(res, host.mem);
}

EXPORT(int64_t, sceClibStrtoll, const char *str, char **endptr, int base) {
    return strtoll(str, endptr, base);
}

EXPORT(int, sceClibTolower, char ch) {
    return tolower(ch);
}

EXPORT(int, sceClibToupper, char ch) {
    return toupper(ch);
}

EXPORT(int, sceClibVdprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibVprintf) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibVsnprintf, char *dst, SceSize dst_max_size, const char *fmt, Address list) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    module::vargs args(list);
    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    int result = utils::snprintf(dst, dst_max_size, fmt, *(thread->cpu), host.mem, args);

    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    return SCE_KERNEL_OK;
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

EXPORT(int, sceIoClose2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoCompleteMultiple) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDevctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDevctlAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDopen, const char *dir) {
    return CALL_EXPORT(_sceIoDopen, dir);
}

EXPORT(int, sceIoDread, const SceUID fd, SceIoDirent *dir) {
    return CALL_EXPORT(_sceIoDread, fd, dir);
}

EXPORT(int, sceIoGetstat, const char *file, SceIoStat *stat) {
    return CALL_EXPORT(_sceIoGetstat, file, stat);
}

EXPORT(int, sceIoGetstatAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetstatByFd, const SceUID fd, SceIoStat *stat) {
    return stat_file_by_fd(host.io, fd, stat, host.pref_path, export_name);
}

EXPORT(int, sceIoIoctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoIoctlAsync) {
    return UNIMPLEMENTED();
}

EXPORT(SceOff, sceIoLseek, const SceUID fd, const SceOff offset, const SceIoSeekMode whence) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    Ptr<_sceIoLseekOpt> options = Ptr<_sceIoLseekOpt>(stack_alloc(*thread->cpu, sizeof(_sceIoLseekOpt)));
    options.get(host.mem)->offset = offset;
    options.get(host.mem)->whence = whence;
    int res = CALL_EXPORT(_sceIoLseek, fd, options);
    stack_free(*thread->cpu, sizeof(_sceIoLseekOpt));
    return res;
}

EXPORT(int, sceIoLseekAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoMkdir, const char *dir, const SceMode mode) {
    return CALL_EXPORT(_sceIoMkdir, dir, mode);
}

EXPORT(int, sceIoMkdirAsync) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceIoOpen, const char *file, const int flags, const SceMode mode) {
    if (file == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    LOG_INFO("Opening file: {}", file);
    return open_file(host.io, file, flags, host.pref_path, export_name);
}

EXPORT(int, sceIoOpenAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoPread, const SceUID fd, void *data, const SceSize size, const SceOff offset) {
    auto pos = tell_file(host.io, fd, export_name);
    if (pos < 0) {
        return static_cast<int>(pos);
    }
    seek_file(fd, static_cast<int>(offset), SCE_SEEK_SET, host.io, export_name);
    const int res = read_file(data, host.io, fd, size, export_name);
    seek_file(fd, static_cast<int>(pos), SCE_SEEK_SET, host.io, export_name);
    return res;
}

EXPORT(int, sceIoPreadAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoPwrite, const SceUID fd, const void *data, const SceSize size, const SceOff offset) {
    auto pos = tell_file(host.io, fd, export_name);
    if (pos < 0) {
        return static_cast<int>(pos);
    }
    seek_file(fd, static_cast<int>(offset), SCE_SEEK_SET, host.io, export_name);
    const int res = write_file(fd, data, size, host.io, export_name);
    seek_file(fd, static_cast<int>(pos), SCE_SEEK_SET, host.io, export_name);
    return res;
}

EXPORT(int, sceIoPwriteAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRead2) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRemove, const char *path) {
    if (path == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    return remove_file(host.io, path, host.pref_path, export_name);
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
    return remove_dir(host.io, path, host.pref_path, export_name);
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

EXPORT(int, sceIoWrite2) {
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

    if (auto error = condvar_create(&uid, host.kernel, export_name, name, thread_id, attr, mutexid, SyncWeight::Heavy)) {
        return error;
    }
    return uid;
}

EXPORT(int, sceKernelCreateEventFlag, const char *name, unsigned int attr, unsigned int flags, SceKernelEventFlagOptParam *opt) {
    return CALL_EXPORT(_sceKernelCreateEventFlag, name, attr, flags, opt);
}

EXPORT(int, sceKernelCreateLwCond, Ptr<SceKernelLwCondWork> workarea, const char *name, SceUInt attr, Ptr<SceKernelLwMutexWork> workarea_mutex, Ptr<SceKernelLwCondOptParam> opt_param) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    Ptr<SceKernelCreateLwCond_opt> options = Ptr<SceKernelCreateLwCond_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateLwCond_opt)));
    options.get(host.mem)->workarea_mutex = workarea_mutex;
    options.get(host.mem)->opt_param = opt_param;
    int res = CALL_EXPORT(_sceKernelCreateLwCond, workarea, name, attr, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateLwCond_opt));
    return res;
}

EXPORT(int, sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, SceUInt attr, int init_count, Ptr<SceKernelLwMutexOptParam> opt_param) {
    return CALL_EXPORT(_sceKernelCreateLwMutex, workarea, name, attr, init_count, opt_param);
}

EXPORT(int, sceKernelCreateMsgPipe, const char *name, uint32_t type, uint32_t attr, SceSize bufSize, const SceKernelCreateMsgPipeOpt *opt) {
    return msgpipe_create(host.kernel, export_name, name, thread_id, attr);
}

EXPORT(int, sceKernelCreateMsgPipeWithLR) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCreateMutex, const char *name, SceUInt attr, int init_count, SceKernelMutexOptParam *opt_param) {
    SceUID uid;

    if (auto error = mutex_create(&uid, host.kernel, host.mem, export_name, name, thread_id, attr, init_count, Ptr<SceKernelLwMutexWork>(0), SyncWeight::Heavy)) {
        return error;
    }
    return uid;
}

EXPORT(int, sceKernelCreateRWLock) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelCreateSema, const char *name, SceUInt attr, int initVal, int maxVal, Ptr<SceKernelSemaOptParam> option) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    Ptr<SceKernelCreateSema_opt> options = Ptr<SceKernelCreateSema_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateSema_opt)));
    options.get(host.mem)->maxVal = maxVal;
    options.get(host.mem)->option = option;
    int res = CALL_EXPORT(_sceKernelCreateSema, name, attr, initVal, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateSema_opt));
    return res;
}

EXPORT(int, sceKernelCreateSema_16XX, const char *name, SceUInt attr, int initVal, int maxVal, Ptr<SceKernelSemaOptParam> option) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    Ptr<SceKernelCreateSema_opt> options = Ptr<SceKernelCreateSema_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateSema_opt)));
    options.get(host.mem)->maxVal = maxVal;
    options.get(host.mem)->option = option;
    int res = CALL_EXPORT(_sceKernelCreateSema, name, attr, initVal, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateSema_opt));
    return res;
}

EXPORT(SceUID, sceKernelCreateSimpleEvent, const char *pName, SceUInt32 attr, SceUInt32 initPattern, const SceKernelSimpleEventOptParam *pOptParam) {
    return eventflag_create(host.kernel, export_name, thread_id, pName, attr, initPattern);
}

EXPORT(SceUID, sceKernelCreateThread, const char *name, SceKernelThreadEntry entry, int init_priority, int stack_size, SceUInt attr, int cpu_affinity_mask, Ptr<SceKernelThreadOptParam> option) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);

    Ptr<SceKernelCreateThread_opt> options = Ptr<SceKernelCreateThread_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateThread_opt)));
    options.get(host.mem)->stack_size = stack_size;
    options.get(host.mem)->attr = attr;
    options.get(host.mem)->cpu_affinity_mask = cpu_affinity_mask;
    options.get(host.mem)->option = option;
    SceUID res = CALL_EXPORT(sceKernelCreateThreadForUser, name, entry, init_priority, options.get(host.mem));
    stack_free(*thread->cpu, sizeof(SceKernelCreateThread_opt));
    return res;
}

EXPORT(SceUID, sceKernelCreateTimer, const char *name, uint32_t flags, const void *next) {
    assert(!next);

    SceUID handle = host.kernel.get_next_uid();

    host.kernel.timers[handle] = std::make_shared<TimerState>();
    TimerPtr &timer_info = host.kernel.timers[handle];

    timer_info->name = name;

    if (flags & static_cast<uint32_t>(TimerFlags::PRIORITY_THREAD))
        timer_info->thread_behaviour = TimerState::ThreadBehaviour::PRIORITY;
    else
        timer_info->thread_behaviour = TimerState::ThreadBehaviour::FIFO;

    if (flags & static_cast<uint32_t>(TimerFlags::AUTOMATIC_RESET))
        timer_info->reset_behaviour = TimerState::ResetBehaviour::AUTOMATIC;
    else
        timer_info->reset_behaviour = TimerState::ResetBehaviour::MANUAL;

    if (flags & static_cast<uint32_t>(TimerFlags::WAKE_ONLY_NOTIFY))
        timer_info->notify_behaviour = TimerState::NotifyBehaviour::ONLY_WAKE;
    else
        timer_info->notify_behaviour = TimerState::NotifyBehaviour::ALL;

    if (flags & static_cast<uint32_t>(TimerFlags::OPENABLE))
        timer_info->openable = true;

    return handle;
}

EXPORT(int, sceKernelDeleteLwCond, Ptr<SceKernelLwCondWork> workarea) {
    SceUID lightweight_condition_id = workarea.get(host.mem)->uid;

    return condvar_delete(host.kernel, export_name, thread_id, lightweight_condition_id, SyncWeight::Light);
}

EXPORT(int, sceKernelDeleteLwMutex, Ptr<SceKernelLwMutexWork> workarea) {
    return CALL_EXPORT(_sceKernelDeleteLwMutex, workarea);
}

EXPORT(int, sceKernelExitProcess, int res) {
    // TODO Handle exit code?
    host.kernel.stop_all_threads();

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

EXPORT(int, sceKernelGetLwMutexInfoById, SceUID lightweight_mutex_id, Ptr<SceKernelLwMutexInfo> info) {
    SceSize size = 0;
    if (info) {
        size = info.get(host.mem)->size;
    }
    return CALL_EXPORT(_sceKernelGetLwMutexInfoById, lightweight_mutex_id, info, size);
}

EXPORT(int, sceKernelGetLwMutexInfo, Ptr<SceKernelLwMutexWork> workarea, Ptr<SceKernelLwMutexInfo> info) {
    return CALL_EXPORT(sceKernelGetLwMutexInfoById, workarea.get(host.mem)->uid, info);
}

EXPORT(int, sceKernelGetModuleInfoByAddr, Ptr<void> addr, SceKernelModuleInfo *info) {
    const auto mod = host.kernel.find_module_by_addr(addr.address());
    if (mod) {
        *info = *mod;
        return SCE_KERNEL_OK;
    }

    return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NOENT);
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
    if (time) {
        *time = rtc_get_ticks(host.kernel.base_tick.tick) - host.kernel.start_tick;
    }
    return 0;
}

EXPORT(SceUInt32, sceKernelGetProcessTimeLow) {
    return static_cast<SceUInt32>(rtc_get_ticks(host.kernel.base_tick.tick) - host.kernel.start_tick);
}

EXPORT(SceUInt64, sceKernelGetProcessTimeWide) {
    return rtc_get_ticks(host.kernel.base_tick.tick) - host.kernel.start_tick;
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
    return host.kernel.get_thread_tls_addr(host.mem, thread_id, key);
}

EXPORT(int, sceKernelGetThreadContextForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadCpuAffinityMask2) {
    STUBBED("STUB");
    return 0x01 << 16;
}

EXPORT(int, sceKernelGetThreadCurrentPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadEventInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadExitStatus, SceUID thid, SceInt32 *pExitStatus) {
    const ThreadStatePtr thread = lock_and_find(thid ? thid : thread_id, host.kernel.threads, host.kernel.mutex);
    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }
    if (thread->status != ThreadStatus::dormant) {
        return SCE_KERNEL_ERROR_NOT_DORMANT;
    }
    if (pExitStatus) {
        *pExitStatus = thread->returned_value;
    }
    return 0;
}

EXPORT(int, sceKernelGetThreadId) {
    return thread_id;
}

EXPORT(int, sceKernelGetThreadInfo, SceUID thid, SceKernelThreadInfo *info) {
    STUBBED("STUB");

    if (!info)
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    // TODO: SCE_KERNEL_ERROR_ILLEGAL_CONTEXT check

    if (info->size > 0x80)
        return SCE_KERNEL_ERROR_NOSYS;

    const ThreadStatePtr thread = lock_and_find(thid ? thid : thread_id, host.kernel.threads, host.kernel.mutex);

    strncpy(info->name, thread->name.c_str(), 0x1f);
    info->stack = Ptr<void>(thread->stack.get());
    info->stackSize = thread->stack_size;
    info->initPriority = thread->priority;
    info->currentPriority = thread->priority;
    info->entry = SceKernelThreadEntry(thread->entry_point);
    info->runClocks = rtc_get_ticks(host.kernel.base_tick.tick) - thread->start_tick;

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetThreadRunStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerBase, SceUID timer_handle, SceKernelSysClock *time) {
    const TimerPtr timer_info = lock_and_find(timer_handle, host.kernel.timers, host.kernel.mutex);

    if (!timer_info)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    *time = timer_info->time;

    return 0;
}

EXPORT(int, sceKernelGetTimerEventRemainingTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerTime, SceUID timer_handle, SceKernelSysClock *time) {
    const TimerPtr timer_info = lock_and_find(timer_handle, host.kernel.timers, host.kernel.mutex);

    if (!timer_info)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    *time = get_current_time() - timer_info->time;

    return 0;
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
bool load_module(SceUID &mod_id, Ptr<const void> &entry_point, SceKernelModuleInfoPtr &module, HostState &host, const char *export_name, const char *path, int &error_val) {
    const auto &loaded_modules = host.kernel.loaded_modules;

    auto module_iter = std::find_if(loaded_modules.begin(), loaded_modules.end(), [path](const auto &p) {
        return std::string(p.second->path) == path;
    });

    if (module_iter == loaded_modules.end()) {
        // module is not loaded, load it here

        const auto file = open_file(host.io, path, SCE_O_RDONLY, host.pref_path, export_name);
        if (file < 0) {
            error_val = RET_ERROR(file);
            return false;
        }
        const auto size = seek_file(file, 0, SCE_SEEK_END, host.io, export_name);
        if (size < 0) {
            error_val = RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
            return false;
        }

        if (seek_file(file, 0, SCE_SEEK_SET, host.io, export_name) < 0) {
            error_val = RET_ERROR(static_cast<int>(size));
            return false;
        }

        auto *data = new char[static_cast<int>(size) + 1]; // null-terminated char array
        if (read_file(data, host.io, file, SceSize(size), export_name) < 0) {
            delete[] data;
            error_val = RET_ERROR(static_cast<int>(size));
            return false;
        }

        mod_id = load_self(entry_point, host.kernel, host.mem, data, path, host.cfg);

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
        entry_point = module->start_entry;
    }
    return true;
}

EXPORT(SceUID, sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option) {
    return CALL_EXPORT(_sceKernelLoadModule, path, flags, option);
}

EXPORT(SceUID, sceKernelLoadStartModule, const char *moduleFileName, SceSize args, const Ptr<void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes) {
    return CALL_EXPORT(_sceKernelLoadStartModule, moduleFileName, args, argp, flags, pOpt, pRes);
}

EXPORT(int, sceKernelLockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    return CALL_EXPORT(_sceKernelLockLwMutex, workarea, lock_count, ptimeout);
}

EXPORT(int, sceKernelLockLwMutexCB, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    STUBBED("no CB");
    return CALL_EXPORT(_sceKernelLockLwMutex, workarea, lock_count, ptimeout);
}

EXPORT(int, sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout) {
    return CALL_EXPORT(_sceKernelLockMutex, mutexid, lock_count, timeout);
}

EXPORT(int, sceKernelLockMutexCB, SceUID mutexid, int lock_count, unsigned int *timeout) {
    STUBBED("no CB");
    return CALL_EXPORT(_sceKernelLockMutex, mutexid, lock_count, timeout);
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

EXPORT(int, sceKernelPollEventFlag, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits) {
    return CALL_EXPORT(_sceKernelPollEventFlag, event_id, flags, wait, outBits);
}

EXPORT(int, sceKernelPrintBacktrace) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPulseEventWithNotifyCallback) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelReceiveMsgPipe, SceUID msgpipe_id, char *recv_buf, SceSize msg_size, SceUInt32 wait_mode, SceSize *result, SceUInt32 *timeout) {
    const auto ret = msgpipe_recv(host.kernel, export_name, thread_id, msgpipe_id, wait_mode, recv_buf, msg_size, timeout);
    if (ret < 0) {
        return ret;
    }
    if (result) {
        *result = ret;
    }
    return SCE_KERNEL_OK;
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

EXPORT(int, sceKernelSendMsgPipe, SceUID msgpipe_id, char *send_buf, SceSize msg_size, SceUInt32 wait_mode, SceSize *result, SceUInt32 *timeout) {
    const auto ret = msgpipe_send(host.kernel, export_name, thread_id, msgpipe_id, wait_mode, send_buf, msg_size, timeout);
    if (ret < 0) {
        return ret;
    }
    if (result) {
        *result = ret;
    }
    return SCE_KERNEL_OK;
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

EXPORT(int, sceKernelSetTimerEvent, SceUID timer_handle, int32_t type, SceKernelSysClock *clock, int32_t repeats) {
    STUBBED("Type not implemented.");

    const TimerPtr timer_info = lock_and_find(timer_handle, host.kernel.timers, host.kernel.mutex);

    if (!timer_info)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    //TODO: Timer values for type.

    timer_info->repeats = repeats;

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelSetTimerTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSignalLwCond, Ptr<SceKernelLwCondWork> workarea) {
    SceUID condid = workarea.get(host.mem)->uid;
    return condvar_signal(host.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Any), SyncWeight::Light);
}

EXPORT(int, sceKernelSignalLwCondAll, Ptr<SceKernelLwCondWork> workarea) {
    SceUID condid = workarea.get(host.mem)->uid;
    return condvar_signal(host.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::All), SyncWeight::Light);
}

EXPORT(int, sceKernelSignalLwCondTo, Ptr<SceKernelLwCondWork> workarea, SceUID thread_target) {
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
    return CALL_EXPORT(_sceKernelStartThread, thid, arglen, argp);
}

EXPORT(int, sceKernelStopModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelStopUnloadModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryLockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int lock_count) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return mutex_try_lock(host.kernel, host.mem, export_name, thread_id, lwmutexid, lock_count, SyncWeight::Light);
}

EXPORT(int, sceKernelTryReceiveMsgPipe, SceUID msgpipe_id, char *recv_buf, SceSize msg_size, SceUInt32 wait_mode, SceSize *result) {
    const auto ret = msgpipe_recv(host.kernel, export_name, thread_id, msgpipe_id, wait_mode | SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT, recv_buf, msg_size, 0);
    if (ret == 0) {
        return SCE_KERNEL_ERROR_MPP_EMPTY;
    }
    if (result) {
        *result = ret;
    }
    return SCE_KERNEL_OK;
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

EXPORT(int, sceKernelUnlockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int unlock_count) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return mutex_unlock(host.kernel, export_name, thread_id, lwmutexid, unlock_count, SyncWeight::Light);
}

EXPORT(int, sceKernelUnlockLwMutex2, Ptr<SceKernelLwMutexWork> workarea, int unlock_count) {
    const auto lwmutexid = workarea.get(host.mem)->uid;
    return mutex_unlock(host.kernel, export_name, thread_id, lwmutexid, unlock_count, SyncWeight::Light);
}

EXPORT(int, sceKernelWaitCond, SceUID cond_id, SceUInt32 *timeout) {
    return condvar_wait(host.kernel, host.mem, export_name, thread_id, cond_id, timeout, SyncWeight::Heavy);
}

EXPORT(int, sceKernelWaitCondCB, SceUID cond_id, SceUInt32 *timeout) {
    STUBBED("no CB");
    return condvar_wait(host.kernel, host.mem, export_name, thread_id, cond_id, timeout, SyncWeight::Heavy);
}

EXPORT(int, sceKernelWaitEvent) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitEventCB) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelWaitEventFlag, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    return eventflag_wait(host.kernel, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat, pTimeout);
}

EXPORT(SceInt32, sceKernelWaitEventFlagCB, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    STUBBED("no CB");
    return eventflag_wait(host.kernel, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat, pTimeout);
}

EXPORT(int, sceKernelWaitException) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitExceptionCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitLwCond, Ptr<SceKernelLwCondWork> workarea, SceUInt32 *timeout) {
    const auto cond_id = workarea.get(host.mem)->uid;
    return condvar_wait(host.kernel, host.mem, export_name, thread_id, cond_id, timeout, SyncWeight::Light);
}

EXPORT(int, sceKernelWaitLwCondCB, Ptr<SceKernelLwCondWork> workarea, SceUInt32 *timeout) {
    STUBBED("no CB");
    const auto cond_id = workarea.get(host.mem)->uid;
    return condvar_wait(host.kernel, host.mem, export_name, thread_id, cond_id, timeout, SyncWeight::Light);
}

EXPORT(int, sceKernelWaitMultipleEvents) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitMultipleEventsCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitSema, SceUID semaid, int signal, SceUInt *timeout) {
    return CALL_EXPORT(_sceKernelWaitSema, semaid, signal, timeout);
}

EXPORT(int, sceKernelWaitSemaCB, SceUID semaid, int signal, SceUInt *timeout) {
    STUBBED("no CB");
    return CALL_EXPORT(_sceKernelWaitSema, semaid, signal, timeout);
}

EXPORT(int, sceKernelWaitSignal, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    return CALL_EXPORT(_sceKernelWaitSignal, unknown, delay, timeout);
}

EXPORT(int, sceKernelWaitSignalCB) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitThreadEnd, SceUID thid, int *stat, SceUInt *timeout) {
    return CALL_EXPORT(_sceKernelWaitThreadEnd, thid, stat, timeout);
}

EXPORT(int, sceKernelWaitThreadEndCB, SceUID thid, int *stat, SceUInt *timeout) {
    return CALL_EXPORT(_sceKernelWaitThreadEndCB, thid, stat, timeout);
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

BRIDGE_IMPL(__sce_aeabi_idiv0)
BRIDGE_IMPL(__sce_aeabi_ldiv0)
BRIDGE_IMPL(__stack_chk_fail)
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
BRIDGE_IMPL(sceIoClose2)
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
BRIDGE_IMPL(sceIoRead2)
BRIDGE_IMPL(sceIoRemove)
BRIDGE_IMPL(sceIoRemoveAsync)
BRIDGE_IMPL(sceIoRename)
BRIDGE_IMPL(sceIoRenameAsync)
BRIDGE_IMPL(sceIoRmdir)
BRIDGE_IMPL(sceIoRmdirAsync)
BRIDGE_IMPL(sceIoSync)
BRIDGE_IMPL(sceIoSyncAsync)
BRIDGE_IMPL(sceIoWrite2)
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
BRIDGE_IMPL(sceKernelGetThreadCpuAffinityMask2)
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
