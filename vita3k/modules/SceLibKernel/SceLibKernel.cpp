// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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
#include <io/functions.h>
#include <kernel/load_self.h>
#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <packages/functions.h>

#include <io/device.h>
#include <io/io.h>
#include <io/types.h>
#include <kernel/types.h>
#include <rtc/rtc.h>
#include <util/lock_and_find.h>
#include <util/log.h>
#include <util/tracy.h>

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

TRACY_MODULE_NAME(SceLibKernel);

inline uint64_t get_current_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

VAR_EXPORT(__sce_libcparam) {
    // This variable almost newer used. So I return something that looks good, but not exacly correct.
    return (emuenv.kernel.process_param ? emuenv.kernel.process_param.get(emuenv.mem)->sce_libc_param.address() : 0);
}

VAR_EXPORT(__stack_chk_guard) {
    auto ptr = Ptr<uint32_t>(alloc(emuenv.mem, 4, "__stack_chk_guard"));
    // Can be any random value
    *ptr.get(emuenv.mem) = 0x0DEADBEE;
    return ptr.address();
}

EXPORT(int, __sce_aeabi_idiv0) {
    TRACY_FUNC(__sce_aeabi_idiv0);
    LOG_ERROR("Division by zero");
    return UNIMPLEMENTED();
}

EXPORT(int, __sce_aeabi_ldiv0) {
    TRACY_FUNC(__sce_aeabi_ldiv0);
    LOG_ERROR("Division by zero");
    return UNIMPLEMENTED();
}

EXPORT(int, __stack_chk_fail) {
    TRACY_FUNC(__stack_chk_fail);
    LOG_CRITICAL("Stack corruption on TID: {}", thread_id);

    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    auto ctx = save_context(*thread->cpu);
    LOG_ERROR("{}", ctx.description());

    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, unsigned int attr, int init_count, Ptr<SceKernelLwMutexOptParam> opt_param) {
    TRACY_FUNC(_sceKernelCreateLwMutex, workarea, name, attr, init_count, opt_param);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    Ptr<SceKernelCreateLwMutex_opt> options = Ptr<SceKernelCreateLwMutex_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateLwMutex_opt)));
    options.get(emuenv.mem)->init_count = init_count;
    options.get(emuenv.mem)->opt_param = opt_param;
    int res = CALL_EXPORT(__sceKernelCreateLwMutex, workarea, name, attr, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateLwMutex_opt));
    return res;
}

EXPORT(int, sceClibAbort) {
    TRACY_FUNC(sceClibAbort);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibDprintf) {
    TRACY_FUNC(sceClibDprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibLookCtypeTable, uint32_t param1) {
    TRACY_FUNC(sceClibLookCtypeTable, param1);
    if (param1 < 128) {
        const uint8_t arr[128] = {
            0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x08, 0x08, 0x08, 0x08, 0x08, 0x20, 0x20,
            0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
            0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
            0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
            0x10, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x10, 0x10, 0x10, 0x10, 0x10,
            0x10, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
            0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x10, 0x10, 0x10, 0x10, 0x20
        };
        return arr[param1];
    } else {
        return 0;
    }
}

EXPORT(int, sceClibMemchr) {
    TRACY_FUNC(sceClibMemchr);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMemcmp, const void *s1, const void *s2, SceSize len) {
    TRACY_FUNC(sceClibMemcmp, s1, s2, len);
    return memcmp(s1, s2, len);
}

EXPORT(int, sceClibMemcmpConstTime) {
    TRACY_FUNC(sceClibMemcmpConstTime);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMemcpy, Ptr<void> dst, const void *src, SceSize len) {
    TRACY_FUNC(sceClibMemcpy, dst, src, len);
    memcpy(dst.get(emuenv.mem), src, len);
    return dst;
}

EXPORT(int, sceClibMemcpyChk) {
    TRACY_FUNC(sceClibMemcpyChk);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMemcpy_safe, Ptr<void> dst, const void *src, SceSize len) {
    TRACY_FUNC(sceClibMemcpy_safe, dst, src, len);
    memcpy(dst.get(emuenv.mem), src, len);
    return dst;
}

EXPORT(Ptr<void>, sceClibMemmove, Ptr<void> dst, const void *src, SceSize len) {
    TRACY_FUNC(sceClibMemmove, dst, src, len);
    memmove(dst.get(emuenv.mem), src, len);
    return dst;
}

EXPORT(int, sceClibMemmoveChk) {
    TRACY_FUNC(sceClibMemmoveChk);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMemset, Ptr<void> dst, int ch, SceSize len) {
    TRACY_FUNC(sceClibMemset, dst, ch, len);
    memset(dst.get(emuenv.mem), ch, len);
    return dst;
}

EXPORT(int, sceClibMemsetChk) {
    TRACY_FUNC(sceClibMemsetChk);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMspaceCalloc, Ptr<void> space, uint32_t elements, uint32_t size) {
    TRACY_FUNC(sceClibMspaceCalloc, space, elements, size);
    const std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);

    void *address = mspace_calloc(space.get(emuenv.mem), elements, size);
    return Ptr<void>(address, emuenv.mem);
}

EXPORT(Ptr<void>, sceClibMspaceCreate, Ptr<void> base, uint32_t capacity) {
    TRACY_FUNC(sceClibMspaceCreate, base, capacity);
    const std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);

    mspace space = create_mspace_with_base(base.get(emuenv.mem), capacity, 0);
    return Ptr<void>(space, emuenv.mem);
}

EXPORT(uint32_t, sceClibMspaceDestroy, Ptr<void> space) {
    TRACY_FUNC(sceClibMspaceDestroy, space);
    const std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);

    return static_cast<uint32_t>(destroy_mspace(space.get(emuenv.mem)));
}

EXPORT(void, sceClibMspaceFree, Ptr<void> space, Ptr<void> address) {
    TRACY_FUNC(sceClibMspaceFree, space, address);
    const std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);

    mspace_free(space.get(emuenv.mem), address.get(emuenv.mem));
}

EXPORT(int, sceClibMspaceIsHeapEmpty) {
    TRACY_FUNC(sceClibMspaceIsHeapEmpty);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMspaceMalloc, Ptr<void> space, uint32_t size) {
    TRACY_FUNC(sceClibMspaceMalloc, space, size);
    const std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);

    void *address = mspace_malloc(space.get(emuenv.mem), size);
    return Ptr<void>(address, emuenv.mem);
}

EXPORT(int, sceClibMspaceMallocStats) {
    TRACY_FUNC(sceClibMspaceMallocStats);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceMallocStatsFast) {
    TRACY_FUNC(sceClibMspaceMallocStatsFast);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibMspaceMallocUsableSize) {
    TRACY_FUNC(sceClibMspaceMallocUsableSize);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<void>, sceClibMspaceMemalign, Ptr<void> space, uint32_t alignment, uint32_t size) {
    TRACY_FUNC(sceClibMspaceMemalign, space, alignment, size);
    const std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);

    void *address = mspace_memalign(space.get(emuenv.mem), alignment, size);
    return Ptr<void>(address, emuenv.mem);
}

EXPORT(Ptr<void>, sceClibMspaceRealloc, Ptr<void> space, Ptr<void> address, uint32_t size) {
    TRACY_FUNC(sceClibMspaceRealloc, space, address, size);
    const std::lock_guard<std::mutex> guard(emuenv.kernel.mutex);

    void *new_address = mspace_realloc(space.get(emuenv.mem), address.get(emuenv.mem), size);
    return Ptr<void>(new_address, emuenv.mem);
}

EXPORT(int, sceClibMspaceReallocalign) {
    TRACY_FUNC(sceClibMspaceReallocalign);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibPrintf, const char *fmt, module::vargs args) {
    TRACY_FUNC(sceClibPrintf, fmt);
    std::vector<char> buffer(KiB(1));

    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    const int result = utils::snprintf(buffer.data(), buffer.size(), fmt, *(thread->cpu), emuenv.mem, args);

    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    LOG_INFO("{}", buffer.data());

    return SCE_KERNEL_OK;
}

EXPORT(int, sceClibSnprintf, char *dst, SceSize dst_max_size, const char *fmt, module::vargs args) {
    TRACY_FUNC(sceClibSnprintf, dst, dst_max_size, fmt);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    int result = utils::snprintf(dst, dst_max_size, fmt, *(thread->cpu), emuenv.mem, args);

    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    return SCE_KERNEL_OK;
}

EXPORT(int, sceClibSnprintfChk) {
    TRACY_FUNC(sceClibSnprintfChk);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrcatChk) {
    TRACY_FUNC(sceClibStrcatChk);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, sceClibStrchr, const char *str, int c) {
    TRACY_FUNC(sceClibStrchr, str, c);
    const char *res = strchr(str, c);
    return Ptr<char>(res, emuenv.mem);
}

EXPORT(int, sceClibStrcmp, const char *s1, const char *s2) {
    TRACY_FUNC(sceClibStrcmp, s1, s2);
    return strcmp(s1, s2);
}

EXPORT(int, sceClibStrcpyChk) {
    TRACY_FUNC(sceClibStrcpyChk);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, sceClibStrlcat, char *dst, const char *src, SceSize len) {
    TRACY_FUNC(sceClibStrlcat, dst, src, len);
    const char *res = strncat(dst, src, len);
    return Ptr<char>(res, emuenv.mem);
}

EXPORT(int, sceClibStrlcatChk) {
    TRACY_FUNC(sceClibStrlcatChk);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<char>, sceClibStrlcpy, char *dst, const char *src, SceSize len) {
    TRACY_FUNC(sceClibStrlcpy, dst, src, len);
    const char *res = strncpy(dst, src, len);
    return Ptr<char>(res, emuenv.mem);
}

EXPORT(int, sceClibStrlcpyChk) {
    TRACY_FUNC(sceClibStrlcpyChk);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncasecmp, const char *s1, const char *s2, SceSize len) {
    TRACY_FUNC(sceClibStrncasecmp, s1, s2, len);
#ifdef WIN32
    return _strnicmp(s1, s2, len);
#else
    return strncasecmp(s1, s2, len);
#endif
}

EXPORT(Ptr<char>, sceClibStrncat, char *dst, const char *src, SceSize len) {
    TRACY_FUNC(sceClibStrncat, dst, src, len);
    const char *res = strncat(dst, src, len);
    return Ptr<char>(res, emuenv.mem);
}

EXPORT(int, sceClibStrncatChk) {
    TRACY_FUNC(sceClibStrncatChk);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibStrncmp, const char *s1, const char *s2, SceSize len) {
    TRACY_FUNC(sceClibStrncmp, s1, s2, len);
    return strncmp(s1, s2, len);
}

EXPORT(Ptr<char>, sceClibStrncpy, char *dst, const char *src, SceSize len) {
    TRACY_FUNC(sceClibStrncpy, dst, src, len);
    const char *res = strncpy(dst, src, len);
    return Ptr<char>(res, emuenv.mem);
}

EXPORT(int, sceClibStrncpyChk) {
    TRACY_FUNC(sceClibStrncpyChk);
    return UNIMPLEMENTED();
}

EXPORT(uint32_t, sceClibStrnlen, const char *s1, SceSize maxlen) {
    TRACY_FUNC(sceClibStrnlen, s1, maxlen);
    return static_cast<uint32_t>(strnlen(s1, maxlen));
}

EXPORT(Ptr<char>, sceClibStrrchr, const char *src, int ch) {
    TRACY_FUNC(sceClibStrrchr, src, ch);
    const char *res = strrchr(src, ch);
    return Ptr<char>(res, emuenv.mem);
}

EXPORT(Ptr<char>, sceClibStrstr, const char *s1, const char *s2) {
    TRACY_FUNC(sceClibStrstr, s1, s2);
    const char *res = strstr(s1, s2);
    return Ptr<char>(res, emuenv.mem);
}

EXPORT(int64_t, sceClibStrtoll, const char *str, char **endptr, int base) {
    TRACY_FUNC(sceClibStrtoll, str, endptr, base);
    return strtoll(str, endptr, base);
}

EXPORT(int, sceClibTolower, char ch) {
    TRACY_FUNC(sceClibTolower, ch);
    return tolower(ch);
}

EXPORT(int, sceClibToupper, char ch) {
    TRACY_FUNC(sceClibToupper, ch);
    return toupper(ch);
}

EXPORT(int, sceClibVdprintf) {
    TRACY_FUNC(sceClibVdprintf);
    return UNIMPLEMENTED();
}

EXPORT(int, sceClibVprintf, const char *fmt, module::vargs args) {
    TRACY_FUNC(sceClibVprintf, fmt);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }
    constexpr int dst_max_size = 1024;
    char dst[dst_max_size];
    int result = utils::snprintf(dst, dst_max_size, fmt, *(thread->cpu), emuenv.mem, args);
    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    if (result == dst_max_size) {
        LOG_WARN("Predefined buffer too small. Result truncated");
    }
    LOG_INFO("{}", dst);
    return SCE_KERNEL_OK;
}

EXPORT(int, sceClibVsnprintf, char *dst, SceSize dst_max_size, const char *fmt, Address list) {
    TRACY_FUNC(sceClibVsnprintf, dst, dst_max_size, fmt, list);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    module::vargs args(list);
    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    int result = utils::snprintf(dst, dst_max_size, fmt, *(thread->cpu), emuenv.mem, args);

    if (!result) {
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    return SCE_KERNEL_OK;
}

EXPORT(int, sceClibVsnprintfChk) {
    TRACY_FUNC(sceClibVsnprintfChk);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoChstat) {
    TRACY_FUNC(sceIoChstat);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoChstatAsync) {
    TRACY_FUNC(sceIoChstatAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoChstatByFd) {
    TRACY_FUNC(sceIoChstatByFd);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoClose2) {
    TRACY_FUNC(sceIoClose2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoCompleteMultiple) {
    TRACY_FUNC(sceIoCompleteMultiple);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDevctl, const char *dev, SceInt cmd, const void *indata, SceSize inlen, void *outdata, SceSize outlen) {
    TRACY_FUNC(sceIoDevctl, dev, cmd, indata, inlen, outdata, outlen);
    if (!dev || !outdata)
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);

    // TODO: Turn the commands into an enum of commands
    switch (cmd) {
    case 12289: { // Get device capacity info?
        assert(outlen == sizeof(SceIoDevInfo));

        auto device = device::get_device(dev);
        if (device == VitaIoDevice::_INVALID) {
            LOG_ERROR("Cannot find device for path: {}", dev);
            return RET_ERROR(SCE_ERROR_ERRNO_ENOENT);
        }

        fs::path dev_path = device._to_string();
        fs::path path = emuenv.pref_path / dev_path;
        fs::space_info space = fs::space(path);

        ((SceIoDevInfo *)outdata)->max_size = space.capacity;
        ((SceIoDevInfo *)outdata)->free_size = space.available;
        STUBBED("cluster size = 4096");
        ((SceIoDevInfo *)outdata)->cluster_size = 4096;
        break;
    }
    default: {
        LOG_WARN("Unhandled case for sceIoDevctl cmd={}", cmd);
        assert(false);
        return 0;
    }
    }

    return 0;
}

EXPORT(int, sceIoDevctlAsync) {
    TRACY_FUNC(sceIoDevctlAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDopen, const char *dir) {
    TRACY_FUNC(sceIoDopen, dir);
    return CALL_EXPORT(_sceIoDopen, dir);
}

EXPORT(int, sceIoDread, const SceUID fd, SceIoDirent *dir) {
    TRACY_FUNC(sceIoDread, fd, dir);
    return CALL_EXPORT(_sceIoDread, fd, dir);
}

EXPORT(int, sceIoGetstat, const char *file, SceIoStat *stat) {
    TRACY_FUNC(sceIoGetstat, file, stat);
    return CALL_EXPORT(_sceIoGetstat, file, stat);
}

EXPORT(int, sceIoGetstatAsync) {
    TRACY_FUNC(sceIoGetstatAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetstatByFd, const SceUID fd, SceIoStat *stat) {
    TRACY_FUNC(sceIoGetstatByFd, fd, stat);
    return stat_file_by_fd(emuenv.io, fd, stat, emuenv.pref_path, export_name);
}

EXPORT(int, sceIoIoctl, SceUID fd, int cmd, const void *argp, SceSize arglen, void *bufp, SceSize buflen) {
    TRACY_FUNC(sceIoIoctl, fd, cmd, argp, arglen, bufp, buflen);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoIoctlAsync) {
    TRACY_FUNC(sceIoIoctlAsync);
    return UNIMPLEMENTED();
}

EXPORT(SceOff, sceIoLseek, const SceUID fd, const SceOff offset, const SceIoSeekMode whence) {
    TRACY_FUNC(sceIoLseek, fd, offset, whence);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    Ptr<_sceIoLseekOpt> options = Ptr<_sceIoLseekOpt>(stack_alloc(*thread->cpu, sizeof(_sceIoLseekOpt)));
    options.get(emuenv.mem)->offset = offset;
    options.get(emuenv.mem)->whence = whence;
    SceOff res = CALL_EXPORT(_sceIoLseek, fd, options);
    stack_free(*thread->cpu, sizeof(_sceIoLseekOpt));
    return res;
}

EXPORT(int, sceIoLseekAsync) {
    TRACY_FUNC(sceIoLseekAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoMkdir, const char *dir, const SceMode mode) {
    TRACY_FUNC(sceIoMkdir, dir, mode);
    return CALL_EXPORT(_sceIoMkdir, dir, mode);
}

EXPORT(int, sceIoMkdirAsync) {
    TRACY_FUNC(sceIoMkdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceIoOpen, const char *file, const int flags, const SceMode mode) {
    TRACY_FUNC(sceIoOpen, file, flags, mode);
    if (file == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    LOG_INFO("Opening file: {}", file);
    return open_file(emuenv.io, file, flags, emuenv.pref_path, export_name);
}

EXPORT(int, sceIoOpenAsync) {
    TRACY_FUNC(sceIoOpenAsync);
    return UNIMPLEMENTED();
}

EXPORT(SceSSize, sceIoPread, SceUID fd, void *buf, SceSize nbyte, SceOff offset) {
    TRACY_FUNC(sceIoPread, fd, buf, nbyte, offset);
    auto pos = tell_file(emuenv.io, fd, export_name);
    if (pos < 0) {
        return static_cast<SceSSize>(pos);
    }
    seek_file(fd, offset, SCE_SEEK_SET, emuenv.io, export_name);
    const auto res = read_file(buf, emuenv.io, fd, nbyte, export_name);
    seek_file(fd, pos, SCE_SEEK_SET, emuenv.io, export_name);
    return res;
}

EXPORT(int, sceIoPreadAsync) {
    TRACY_FUNC(sceIoPreadAsync);
    return UNIMPLEMENTED();
}

EXPORT(SceSSize, sceIoPwrite, SceUID fd, const void *buf, SceSize nbyte, SceOff offset) {
    TRACY_FUNC(sceIoPwrite, fd, buf, nbyte, offset);
    auto pos = tell_file(emuenv.io, fd, export_name);
    if (pos < 0) {
        return static_cast<SceSSize>(pos);
    }
    seek_file(fd, offset, SCE_SEEK_SET, emuenv.io, export_name);
    const auto res = write_file(fd, buf, nbyte, emuenv.io, export_name);
    seek_file(fd, pos, SCE_SEEK_SET, emuenv.io, export_name);
    return res;
}

EXPORT(int, sceIoPwriteAsync) {
    TRACY_FUNC(sceIoPwriteAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRead2) {
    TRACY_FUNC(sceIoRead2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRemove, const char *path) {
    TRACY_FUNC(sceIoRemove, path);
    if (path == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    return remove_file(emuenv.io, path, emuenv.pref_path, export_name);
}

EXPORT(int, sceIoRemoveAsync) {
    TRACY_FUNC(sceIoRemoveAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRename, const char *oldname, const char *newname) {
    TRACY_FUNC(sceIoRename, oldname, newname);
    if (!oldname || !newname)
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);

    LOG_INFO("Renaming: {} to {}", oldname, newname);
    return rename(emuenv.io, oldname, newname, emuenv.pref_path, export_name);
}

EXPORT(int, sceIoRenameAsync) {
    TRACY_FUNC(sceIoRenameAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoRmdir, const char *path) {
    TRACY_FUNC(sceIoRmdir, path);
    if (path == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    return remove_dir(emuenv.io, path, emuenv.pref_path, export_name);
}

EXPORT(int, sceIoRmdirAsync) {
    TRACY_FUNC(sceIoRmdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSync) {
    TRACY_FUNC(sceIoSync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSyncAsync) {
    TRACY_FUNC(sceIoSyncAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoWrite2) {
    TRACY_FUNC(sceIoWrite2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddAndGet16) {
    TRACY_FUNC(sceKernelAtomicAddAndGet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddAndGet32) {
    TRACY_FUNC(sceKernelAtomicAddAndGet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddAndGet64) {
    TRACY_FUNC(sceKernelAtomicAddAndGet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddAndGet8) {
    TRACY_FUNC(sceKernelAtomicAddAndGet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddUnless16) {
    TRACY_FUNC(sceKernelAtomicAddUnless16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddUnless32) {
    TRACY_FUNC(sceKernelAtomicAddUnless32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddUnless64) {
    TRACY_FUNC(sceKernelAtomicAddUnless64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAddUnless8) {
    TRACY_FUNC(sceKernelAtomicAddUnless8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAndAndGet16) {
    TRACY_FUNC(sceKernelAtomicAndAndGet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAndAndGet32) {
    TRACY_FUNC(sceKernelAtomicAndAndGet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAndAndGet64) {
    TRACY_FUNC(sceKernelAtomicAndAndGet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicAndAndGet8) {
    TRACY_FUNC(sceKernelAtomicAndAndGet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearAndGet16) {
    TRACY_FUNC(sceKernelAtomicClearAndGet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearAndGet32) {
    TRACY_FUNC(sceKernelAtomicClearAndGet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearAndGet64) {
    TRACY_FUNC(sceKernelAtomicClearAndGet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearAndGet8) {
    TRACY_FUNC(sceKernelAtomicClearAndGet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearMask16) {
    TRACY_FUNC(sceKernelAtomicClearMask16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearMask32) {
    TRACY_FUNC(sceKernelAtomicClearMask32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearMask64) {
    TRACY_FUNC(sceKernelAtomicClearMask64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicClearMask8) {
    TRACY_FUNC(sceKernelAtomicClearMask8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicCompareAndSet16) {
    TRACY_FUNC(sceKernelAtomicCompareAndSet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicCompareAndSet32) {
    TRACY_FUNC(sceKernelAtomicCompareAndSet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicCompareAndSet64) {
    TRACY_FUNC(sceKernelAtomicCompareAndSet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicCompareAndSet8) {
    TRACY_FUNC(sceKernelAtomicCompareAndSet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicDecIfPositive16) {
    TRACY_FUNC(sceKernelAtomicDecIfPositive16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicDecIfPositive32) {
    TRACY_FUNC(sceKernelAtomicDecIfPositive32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicDecIfPositive64) {
    TRACY_FUNC(sceKernelAtomicDecIfPositive64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicDecIfPositive8) {
    TRACY_FUNC(sceKernelAtomicDecIfPositive8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAdd16) {
    TRACY_FUNC(sceKernelAtomicGetAndAdd16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAdd32) {
    TRACY_FUNC(sceKernelAtomicGetAndAdd32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAdd64) {
    TRACY_FUNC(sceKernelAtomicGetAndAdd64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAdd8) {
    TRACY_FUNC(sceKernelAtomicGetAndAdd8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAnd16) {
    TRACY_FUNC(sceKernelAtomicGetAndAnd16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAnd32) {
    TRACY_FUNC(sceKernelAtomicGetAndAnd32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAnd64) {
    TRACY_FUNC(sceKernelAtomicGetAndAnd64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndAnd8) {
    TRACY_FUNC(sceKernelAtomicGetAndAnd8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndClear16) {
    TRACY_FUNC(sceKernelAtomicGetAndClear16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndClear32) {
    TRACY_FUNC(sceKernelAtomicGetAndClear32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndClear64) {
    TRACY_FUNC(sceKernelAtomicGetAndClear64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndClear8) {
    TRACY_FUNC(sceKernelAtomicGetAndClear8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndOr16) {
    TRACY_FUNC(sceKernelAtomicGetAndOr16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndOr32) {
    TRACY_FUNC(sceKernelAtomicGetAndOr32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndOr64) {
    TRACY_FUNC(sceKernelAtomicGetAndOr64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndOr8) {
    TRACY_FUNC(sceKernelAtomicGetAndOr8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSet16) {
    TRACY_FUNC(sceKernelAtomicGetAndSet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSet32) {
    TRACY_FUNC(sceKernelAtomicGetAndSet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSet64) {
    TRACY_FUNC(sceKernelAtomicGetAndSet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSet8) {
    TRACY_FUNC(sceKernelAtomicGetAndSet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSub16) {
    TRACY_FUNC(sceKernelAtomicGetAndSub16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSub32) {
    TRACY_FUNC(sceKernelAtomicGetAndSub32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSub64) {
    TRACY_FUNC(sceKernelAtomicGetAndSub64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndSub8) {
    TRACY_FUNC(sceKernelAtomicGetAndSub8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndXor16) {
    TRACY_FUNC(sceKernelAtomicGetAndXor16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndXor32) {
    TRACY_FUNC(sceKernelAtomicGetAndXor32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndXor64) {
    TRACY_FUNC(sceKernelAtomicGetAndXor64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicGetAndXor8) {
    TRACY_FUNC(sceKernelAtomicGetAndXor8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicOrAndGet16) {
    TRACY_FUNC(sceKernelAtomicOrAndGet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicOrAndGet32) {
    TRACY_FUNC(sceKernelAtomicOrAndGet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicOrAndGet64) {
    TRACY_FUNC(sceKernelAtomicOrAndGet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicOrAndGet8) {
    TRACY_FUNC(sceKernelAtomicOrAndGet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSet16) {
    TRACY_FUNC(sceKernelAtomicSet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSet32) {
    TRACY_FUNC(sceKernelAtomicSet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSet64) {
    TRACY_FUNC(sceKernelAtomicSet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSet8) {
    TRACY_FUNC(sceKernelAtomicSet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSubAndGet16) {
    TRACY_FUNC(sceKernelAtomicSubAndGet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSubAndGet32) {
    TRACY_FUNC(sceKernelAtomicSubAndGet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSubAndGet64) {
    TRACY_FUNC(sceKernelAtomicSubAndGet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicSubAndGet8) {
    TRACY_FUNC(sceKernelAtomicSubAndGet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicXorAndGet16) {
    TRACY_FUNC(sceKernelAtomicXorAndGet16);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicXorAndGet32) {
    TRACY_FUNC(sceKernelAtomicXorAndGet32);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicXorAndGet64) {
    TRACY_FUNC(sceKernelAtomicXorAndGet64);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAtomicXorAndGet8) {
    TRACY_FUNC(sceKernelAtomicXorAndGet8);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelBacktrace) {
    TRACY_FUNC(sceKernelBacktrace);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelBacktraceSelf) {
    TRACY_FUNC(sceKernelBacktraceSelf);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCallModuleExit) {
    TRACY_FUNC(sceKernelCallModuleExit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCallWithChangeStack) {
    TRACY_FUNC(sceKernelCallWithChangeStack);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelEvent) {
    TRACY_FUNC(sceKernelCancelEvent);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelCancelEventFlag, SceUID event_id, SceUInt pattern, SceUInt32 *num_wait_thread) {
    TRACY_FUNC(sceKernelCancelEventFlag, event_id, pattern, num_wait_thread);
    return CALL_EXPORT(_sceKernelCancelEventFlag, event_id, pattern, num_wait_thread);
}

EXPORT(int, sceKernelCancelEventWithSetPattern) {
    TRACY_FUNC(sceKernelCancelEventWithSetPattern);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelMsgPipe) {
    TRACY_FUNC(sceKernelCancelMsgPipe);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelMutex) {
    TRACY_FUNC(sceKernelCancelMutex);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelRWLock) {
    TRACY_FUNC(sceKernelCancelRWLock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCancelSema, SceUID semaId, SceInt32 setCount, SceUInt32 *pNumWaitThreads) {
    TRACY_FUNC(sceKernelCancelSema, semaId, setCount, pNumWaitThreads);
    return CALL_EXPORT(_sceKernelCancelSema, semaId, setCount, pNumWaitThreads);
}

EXPORT(int, sceKernelCancelTimer) {
    TRACY_FUNC(sceKernelCancelTimer);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelChangeCurrentThreadAttr) {
    TRACY_FUNC(sceKernelChangeCurrentThreadAttr);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCheckThreadStack) {
    TRACY_FUNC(sceKernelCheckThreadStack);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);
    int stack_size = read_sp(*thread->cpu) - (thread->stack.get());
    if (stack_size < 0 || stack_size > thread->stack_size)
        stack_size = 0;
    return stack_size;
}

EXPORT(int, sceKernelCloseModule) {
    TRACY_FUNC(sceKernelCloseModule);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelCreateCond, const char *pName, SceUInt32 attr, SceUID mutexId, const SceKernelCondOptParam *pOptParam) {
    TRACY_FUNC(sceKernelCreateCond, pName, attr, mutexId, pOptParam);
    return CALL_EXPORT(_sceKernelCreateCond, pName, attr, mutexId, pOptParam);
}

EXPORT(int, sceKernelCreateEventFlag, const char *name, unsigned int attr, unsigned int flags, SceKernelEventFlagOptParam *opt) {
    TRACY_FUNC(sceKernelCreateEventFlag, name, attr, flags, opt);
    return CALL_EXPORT(_sceKernelCreateEventFlag, name, attr, flags, opt);
}

EXPORT(int, sceKernelCreateLwCond, Ptr<SceKernelLwCondWork> workarea, const char *name, SceUInt attr, Ptr<SceKernelLwMutexWork> workarea_mutex, Ptr<SceKernelLwCondOptParam> opt_param) {
    TRACY_FUNC(sceKernelCreateLwCond, workarea, name, attr, workarea_mutex, opt_param);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    Ptr<SceKernelCreateLwCond_opt> options = Ptr<SceKernelCreateLwCond_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateLwCond_opt)));
    options.get(emuenv.mem)->workarea_mutex = workarea_mutex;
    options.get(emuenv.mem)->opt_param = opt_param;
    int res = CALL_EXPORT(_sceKernelCreateLwCond, workarea, name, attr, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateLwCond_opt));
    return res;
}

EXPORT(int, sceKernelCreateLwMutex, Ptr<SceKernelLwMutexWork> workarea, const char *name, SceUInt attr, int init_count, Ptr<SceKernelLwMutexOptParam> opt_param) {
    TRACY_FUNC(sceKernelCreateLwMutex, workarea, name, attr, init_count, opt_param);
    return CALL_EXPORT(_sceKernelCreateLwMutex, workarea, name, attr, init_count, opt_param);
}

EXPORT(int, sceKernelCreateMsgPipe, const char *name, uint32_t type, uint32_t attr, SceSize bufSize, const SceKernelCreateMsgPipeOpt *opt) {
    TRACY_FUNC(sceKernelCreateMsgPipe, name, type, attr, bufSize, opt);
    return msgpipe_create(emuenv.kernel, export_name, name, thread_id, attr, bufSize);
}

EXPORT(int, sceKernelCreateMsgPipeWithLR) {
    TRACY_FUNC(sceKernelCreateMsgPipeWithLR);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCreateMutex, const char *name, SceUInt attr, int init_count, SceKernelMutexOptParam *opt_param) {
    TRACY_FUNC(sceKernelCreateMutex, name, attr, init_count, opt_param);
    if ((attr & SCE_KERNEL_MUTEX_ATTR_CEILING)) {
        STUBBED("priority ceiling feature is not supported");
    }

    SceUID uid;
    if (auto error = mutex_create(&uid, emuenv.kernel, emuenv.mem, export_name, name, thread_id, attr, init_count, Ptr<SceKernelLwMutexWork>(0), SyncWeight::Heavy)) {
        return error;
    }
    return uid;
}

EXPORT(SceUID, sceKernelCreateRWLock, const char *name, SceUInt32 attr, SceKernelMutexOptParam *opt_param) {
    TRACY_FUNC(sceKernelCreateRWLock, name, attr, opt_param);
    return CALL_EXPORT(_sceKernelCreateRWLock, name, attr, opt_param);
}

EXPORT(SceUID, sceKernelCreateSema, const char *name, SceUInt attr, int initVal, int maxVal, Ptr<SceKernelSemaOptParam> option) {
    TRACY_FUNC(sceKernelCreateSema, name, attr, initVal, maxVal, option);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    Ptr<SceKernelCreateSema_opt> options = Ptr<SceKernelCreateSema_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateSema_opt)));
    options.get(emuenv.mem)->maxVal = maxVal;
    options.get(emuenv.mem)->option = option;
    int res = CALL_EXPORT(_sceKernelCreateSema, name, attr, initVal, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateSema_opt));
    return res;
}

EXPORT(int, sceKernelCreateSema_16XX, const char *name, SceUInt attr, int initVal, int maxVal, Ptr<SceKernelSemaOptParam> option) {
    TRACY_FUNC(sceKernelCreateSema_16XX, name, attr, initVal, maxVal, option);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    Ptr<SceKernelCreateSema_opt> options = Ptr<SceKernelCreateSema_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateSema_opt)));
    options.get(emuenv.mem)->maxVal = maxVal;
    options.get(emuenv.mem)->option = option;
    int res = CALL_EXPORT(_sceKernelCreateSema, name, attr, initVal, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateSema_opt));
    return res;
}

EXPORT(SceUID, sceKernelCreateSimpleEvent, const char *name, SceUInt32 attr, SceUInt32 init_pattern, const SceKernelSimpleEventOptParam *pOptParam) {
    TRACY_FUNC(sceKernelCreateSimpleEvent, name, attr, init_pattern, pOptParam);
    return CALL_EXPORT(_sceKernelCreateSimpleEvent, name, attr, init_pattern, pOptParam);
}

EXPORT(SceUID, sceKernelCreateThread, const char *name, SceKernelThreadEntry entry, int init_priority, int stack_size, SceUInt attr, int cpu_affinity_mask, Ptr<SceKernelThreadOptParam> option) {
    TRACY_FUNC(sceKernelCreateThread, name, entry, init_priority, stack_size, attr, cpu_affinity_mask, option);
    const ThreadStatePtr thread = lock_and_find(thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);

    auto options = Ptr<SceKernelCreateThread_opt>(stack_alloc(*thread->cpu, sizeof(SceKernelCreateThread_opt))).get(emuenv.mem);
    options->stack_size = stack_size;
    options->attr = attr;
    options->cpu_affinity_mask = cpu_affinity_mask;
    options->option = option;
    SceUID res = CALL_EXPORT(sceKernelCreateThreadForUser, name, entry, init_priority, options);
    stack_free(*thread->cpu, sizeof(SceKernelCreateThread_opt));
    return res;
}

EXPORT(SceUID, sceKernelCreateTimer, const char *name, SceUInt32 attr, const uint32_t *opt_params) {
    TRACY_FUNC(sceKernelCreateTimer, name, attr, opt_params);
    return CALL_EXPORT(_sceKernelCreateTimer, name, attr, opt_params);
}

EXPORT(int, sceKernelDeleteLwCond, Ptr<SceKernelLwCondWork> workarea) {
    TRACY_FUNC(sceKernelDeleteLwCond, workarea);
    SceUID lightweight_condition_id = workarea.get(emuenv.mem)->uid;

    return condvar_delete(emuenv.kernel, export_name, thread_id, lightweight_condition_id, SyncWeight::Light);
}

EXPORT(int, sceKernelDeleteLwMutex, Ptr<SceKernelLwMutexWork> workarea) {
    TRACY_FUNC(sceKernelDeleteLwMutex, workarea);
    return CALL_EXPORT(_sceKernelDeleteLwMutex, workarea);
}

EXPORT(int, sceKernelExitProcess, int res) {
    TRACY_FUNC(sceKernelExitProcess, res);
    // TODO Handle exit code?
    emuenv.kernel.exit_delete_all_threads();

    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, sceKernelGetCallbackInfo, SceUID callbackId, Ptr<SceKernelCallbackInfo> pInfo) {
    TRACY_FUNC(sceKernelGetCallbackInfo, callbackId, pInfo);
    return CALL_EXPORT(_sceKernelGetCallbackInfo, callbackId, pInfo.get(emuenv.mem));
}

EXPORT(SceInt32, sceKernelGetCondInfo, SceUID condId, Ptr<SceKernelCondInfo> pInfo) {
    TRACY_FUNC(sceKernelGetCondInfo, condId, pInfo);
    return CALL_EXPORT(_sceKernelGetCondInfo, condId, pInfo);
}

EXPORT(int, sceKernelGetCurrentThreadVfpException) {
    TRACY_FUNC(sceKernelGetCurrentThreadVfpException);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelGetEventFlagInfo, SceUID evfId, Ptr<SceKernelEventFlagInfo> pInfo) {
    TRACY_FUNC(sceKernelGetEventFlagInfo, evfId, pInfo);
    return CALL_EXPORT(_sceKernelGetEventFlagInfo, evfId, pInfo);
}

EXPORT(int, sceKernelGetEventInfo) {
    TRACY_FUNC(sceKernelGetEventInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetEventPattern) {
    TRACY_FUNC(sceKernelGetEventPattern);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLwCondInfo) {
    TRACY_FUNC(sceKernelGetLwCondInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLwCondInfoById) {
    TRACY_FUNC(sceKernelGetLwCondInfoById);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLwMutexInfoById, SceUID lightweight_mutex_id, Ptr<SceKernelLwMutexInfo> info) {
    TRACY_FUNC(sceKernelGetLwMutexInfoById, lightweight_mutex_id, info);
    SceSize size = 0;
    if (info) {
        size = info.get(emuenv.mem)->size;
    }
    return CALL_EXPORT(_sceKernelGetLwMutexInfoById, lightweight_mutex_id, info, size);
}

EXPORT(int, sceKernelGetLwMutexInfo, Ptr<SceKernelLwMutexWork> workarea, Ptr<SceKernelLwMutexInfo> info) {
    TRACY_FUNC(sceKernelGetLwMutexInfo, workarea, info);
    return CALL_EXPORT(sceKernelGetLwMutexInfoById, workarea.get(emuenv.mem)->uid, info);
}

EXPORT(int, sceKernelGetModuleInfoByAddr, Ptr<void> addr, SceKernelModuleInfo *info) {
    TRACY_FUNC(sceKernelGetModuleInfoByAddr, addr, info);
    const auto mod = emuenv.kernel.find_module_by_addr(addr.address());
    if (mod) {
        *info = *mod;
        return SCE_KERNEL_OK;
    }

    return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NOENT);
}

EXPORT(int, sceKernelGetMsgPipeInfo) {
    TRACY_FUNC(sceKernelGetMsgPipeInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetMutexInfo, SceUID mutexId, SceKernelMutexInfo *info) {
    TRACY_FUNC(sceKernelGetMutexInfo, mutexId, info);
    return CALL_EXPORT(_sceKernelGetMutexInfo, mutexId, info);
}

EXPORT(int, sceKernelGetOpenPsId) {
    TRACY_FUNC(sceKernelGetOpenPsId);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetPMUSERENR) {
    TRACY_FUNC(sceKernelGetPMUSERENR);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetProcessTime, SceUInt64 *time) {
    TRACY_FUNC(sceKernelGetProcessTime, time);
    if (time) {
        *time = rtc_get_ticks(emuenv.kernel.base_tick.tick) - emuenv.kernel.start_tick;
    }
    return 0;
}

EXPORT(SceUInt32, sceKernelGetProcessTimeLow) {
    TRACY_FUNC(sceKernelGetProcessTimeLow);
    return static_cast<SceUInt32>(rtc_get_ticks(emuenv.kernel.base_tick.tick) - emuenv.kernel.start_tick);
}

EXPORT(SceUInt64, sceKernelGetProcessTimeWide) {
    TRACY_FUNC(sceKernelGetProcessTimeWide);
    return rtc_get_ticks(emuenv.kernel.base_tick.tick) - emuenv.kernel.start_tick;
}

EXPORT(int, sceKernelGetRWLockInfo, SceUID rwlockId, SceKernelRWLockInfo *info) {
    TRACY_FUNC(sceKernelGetRWLockInfo, rwlockId, info);
    return CALL_EXPORT(_sceKernelGetRWLockInfo, rwlockId, info);
}

EXPORT(SceInt32, sceKernelGetSemaInfo, SceUID semaId, Ptr<SceKernelSemaInfo> pInfo) {
    TRACY_FUNC(sceKernelGetSemaInfo, semaId, pInfo);
    return CALL_EXPORT(_sceKernelGetSemaInfo, semaId, pInfo);
}

EXPORT(int, sceKernelGetSystemInfo) {
    TRACY_FUNC(sceKernelGetSystemInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetSystemTime) {
    TRACY_FUNC(sceKernelGetSystemTime);
    return UNIMPLEMENTED();
}

EXPORT(Ptr<Ptr<void>>, sceKernelGetTLSAddr, int key) {
    TRACY_FUNC(sceKernelGetTLSAddr, key);
    return emuenv.kernel.get_thread_tls_addr(emuenv.mem, thread_id, key);
}

EXPORT(int, sceKernelGetThreadContextForVM, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    TRACY_FUNC(sceKernelGetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
    return CALL_EXPORT(_sceKernelGetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
}

EXPORT(SceInt32, sceKernelGetThreadCpuAffinityMask2, SceUID thid) {
    TRACY_FUNC(sceKernelGetThreadCpuAffinityMask2, thid);
    const SceInt32 affinity = CALL_EXPORT(_sceKernelGetThreadCpuAffinityMask, thid);

    // I guess the difference between this function and sceKernelGetThreadCpuAffinityMask
    // is that if the result is SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT (=0)
    // it gets converted into its real affinity (SCE_KERNEL_CPU_MASK_USER_ALL)
    // either way without this dragon quest builder does not boot
    if (affinity == SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT)
        return SCE_KERNEL_CPU_MASK_USER_ALL;

    return affinity;
}

EXPORT(SceInt32, sceKernelGetThreadCurrentPriority) {
    TRACY_FUNC(sceKernelGetThreadCurrentPriority);
    const ThreadStatePtr thread = emuenv.kernel.get_thread(thread_id);

    if (!thread)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);

    return thread->priority;
}

EXPORT(int, sceKernelGetThreadEventInfo) {
    TRACY_FUNC(sceKernelGetThreadEventInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetThreadExitStatus, SceUID thid, SceInt32 *pExitStatus) {
    TRACY_FUNC(sceKernelGetThreadExitStatus, thid, pExitStatus);
    const ThreadStatePtr thread = lock_and_find(thid ? thid : thread_id, emuenv.kernel.threads, emuenv.kernel.mutex);
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
    TRACY_FUNC(sceKernelGetThreadId);
    return thread_id;
}

EXPORT(SceInt32, sceKernelGetThreadInfo, SceUID threadId, Ptr<SceKernelThreadInfo> pInfo) {
    TRACY_FUNC(sceKernelGetThreadInfo, threadId, pInfo);
    return CALL_EXPORT(_sceKernelGetThreadInfo, threadId, pInfo);
}

EXPORT(int, sceKernelGetThreadRunStatus) {
    TRACY_FUNC(sceKernelGetThreadRunStatus);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerBase, SceUID timer_handle, SceKernelSysClock *time) {
    TRACY_FUNC(sceKernelGetTimerBase, timer_handle, time);
    const TimerPtr timer_info = lock_and_find(timer_handle, emuenv.kernel.timers, emuenv.kernel.mutex);

    if (!timer_info)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    *time = timer_info->time;

    return 0;
}

EXPORT(int, sceKernelGetTimerEventRemainingTime) {
    TRACY_FUNC(sceKernelGetTimerEventRemainingTime);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerInfo) {
    TRACY_FUNC(sceKernelGetTimerInfo);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetTimerTime, SceUID timer_handle, SceKernelSysClock *time) {
    TRACY_FUNC(sceKernelGetTimerTime, timer_handle, time);
    const TimerPtr timer_info = lock_and_find(timer_handle, emuenv.kernel.timers, emuenv.kernel.mutex);

    if (!timer_info)
        return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;

    *time = get_current_time() - timer_info->time;

    return 0;
}

EXPORT(SceUID, sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option) {
    TRACY_FUNC(sceKernelLoadModule, path, flags, option);
    return CALL_EXPORT(_sceKernelLoadModule, path, flags, option);
}

EXPORT(SceUID, sceKernelLoadStartModule, const char *moduleFileName, SceSize args, const Ptr<void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes) {
    TRACY_FUNC(sceKernelLoadStartModule, moduleFileName, args, argp, flags, pOpt, pRes);
    return CALL_EXPORT(_sceKernelLoadStartModule, moduleFileName, args, argp, flags, pOpt, pRes);
}

EXPORT(int, sceKernelLockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    TRACY_FUNC(sceKernelLockLwMutex, workarea, lock_count, ptimeout);
    return CALL_EXPORT(_sceKernelLockLwMutex, workarea, lock_count, ptimeout);
}

EXPORT(int, sceKernelLockLwMutex_0, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    TRACY_FUNC(sceKernelLockLwMutex_0, workarea, lock_count, ptimeout);
    return CALL_EXPORT(_sceKernelLockLwMutex, workarea, lock_count, ptimeout);
}

EXPORT(int, sceKernelLockLwMutexCB, Ptr<SceKernelLwMutexWork> workarea, int lock_count, unsigned int *ptimeout) {
    TRACY_FUNC(sceKernelLockLwMutexCB, workarea, lock_count, ptimeout);
    process_callbacks(emuenv.kernel, thread_id);
    return CALL_EXPORT(_sceKernelLockLwMutex, workarea, lock_count, ptimeout);
}

EXPORT(int, sceKernelLockMutex, SceUID mutexid, int lock_count, unsigned int *timeout) {
    TRACY_FUNC(sceKernelLockMutex, mutexid, lock_count, timeout);
    return CALL_EXPORT(_sceKernelLockMutex, mutexid, lock_count, timeout);
}

EXPORT(SceInt32, sceKernelLockMutexCB, SceUID mutexId, SceInt32 lockCount, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelLockMutexCB, mutexId, lockCount, pTimeout);
    return CALL_EXPORT(_sceKernelLockMutexCB, mutexId, lockCount, pTimeout);
}

EXPORT(SceInt32, sceKernelLockReadRWLock, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(sceKernelLockReadRWLock, lock_id, timeout);
    return CALL_EXPORT(_sceKernelLockReadRWLock, lock_id, timeout);
}

EXPORT(SceInt32, sceKernelLockReadRWLockCB, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(sceKernelLockReadRWLockCB, lock_id, timeout);
    return CALL_EXPORT(_sceKernelLockReadRWLockCB, lock_id, timeout);
}

EXPORT(SceInt32, sceKernelLockWriteRWLock, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(sceKernelLockWriteRWLock, lock_id, timeout);
    return CALL_EXPORT(_sceKernelLockWriteRWLock, lock_id, timeout);
}

EXPORT(SceInt32, sceKernelLockWriteRWLockCB, SceUID lock_id, SceUInt32 *timeout) {
    TRACY_FUNC(sceKernelLockWriteRWLockCB, lock_id, timeout);
    return CALL_EXPORT(_sceKernelLockWriteRWLockCB, lock_id, timeout);
}

EXPORT(SceUID, sceKernelOpenModule, const char *moduleFileName, SceSize args, const Ptr<void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes) {
    TRACY_FUNC(sceKernelOpenModule, moduleFileName, args, argp, flags, pOpt, pRes);
    return CALL_EXPORT(_sceKernelLoadStartModule, moduleFileName, args, argp, flags, pOpt, pRes);
}

EXPORT(int, sceKernelPMonThreadGetCounter) {
    TRACY_FUNC(sceKernelPMonThreadGetCounter);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelPollEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data) {
    TRACY_FUNC(sceKernelPollEvent, event_id, bit_pattern, result_pattern, user_data);
    return CALL_EXPORT(_sceKernelPollEvent, event_id, bit_pattern, result_pattern, user_data);
}

EXPORT(int, sceKernelPollEventFlag, SceUID event_id, unsigned int flags, unsigned int wait, unsigned int *outBits) {
    TRACY_FUNC(sceKernelPollEventFlag, event_id, flags, wait, outBits);
    return CALL_EXPORT(_sceKernelPollEventFlag, event_id, flags, wait, outBits);
}

EXPORT(int, sceKernelPrintBacktrace) {
    TRACY_FUNC(sceKernelPrintBacktrace);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelPulseEventWithNotifyCallback) {
    TRACY_FUNC(sceKernelPulseEventWithNotifyCallback);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelReceiveMsgPipe, SceUID msgPipeId, void *pRecvBuf, SceSize recvSize, SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelReceiveMsgPipe, msgPipeId, pRecvBuf, recvSize, waitMode, pResult, pTimeout);
    const auto ret = msgpipe_recv(emuenv.kernel, export_name, thread_id, msgPipeId, waitMode, pRecvBuf, recvSize, pTimeout);
    if (static_cast<int>(ret) < 0) {
        return ret;
    }
    if (pResult) {
        *pResult = ret;
    }
    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, sceKernelReceiveMsgPipeCB, SceUID msgPipeId, void *pRecvBuf, SceSize recvSize, SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelReceiveMsgPipeCB, msgPipeId, pRecvBuf, recvSize, waitMode, pResult, pTimeout);
    process_callbacks(emuenv.kernel, thread_id);
    return CALL_EXPORT(sceKernelReceiveMsgPipe, msgPipeId, pRecvBuf, recvSize, waitMode, pResult, pTimeout);
}

EXPORT(int, sceKernelReceiveMsgPipeVector) {
    TRACY_FUNC(sceKernelReceiveMsgPipeVector);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelReceiveMsgPipeVectorCB) {
    TRACY_FUNC(sceKernelReceiveMsgPipeVectorCB);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelRegisterThreadEventHandler, const char *name, SceUID thread_mask, SceUInt32 mask, Ptr<const void> handler, Address common) {
    TRACY_FUNC(sceKernelRegisterThreadEventHandler, name, thread_mask, mask, handler, common);
    sceKernelRegisterThreadEventHandlerOpt handler_opt = {
        .handler = handler,
        .common = common
    };
    return CALL_EXPORT(_sceKernelRegisterThreadEventHandler, name, thread_mask, mask, &handler_opt);
}

EXPORT(SceInt32, sceKernelSendMsgPipe, SceUID msgPipeId, const void *pSendBuf, SceSize sendSize, SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelSendMsgPipe, msgPipeId, pSendBuf, sendSize, waitMode, pResult, pTimeout);
    const auto ret = msgpipe_send(emuenv.kernel, export_name, thread_id, msgPipeId, waitMode, pSendBuf, sendSize, pTimeout);
    if (static_cast<int>(ret) < 0) {
        return ret;
    }
    if (pResult) {
        *pResult = ret;
    }
    return SCE_KERNEL_OK;
}

EXPORT(SceInt32, sceKernelSendMsgPipeCB, SceUID msgPipeId, const void *pSendBuf, SceSize sendSize, SceUInt32 waitMode, SceSize *pResult, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelSendMsgPipeCB, msgPipeId, pSendBuf, sendSize, waitMode, pResult, pTimeout);
    process_callbacks(emuenv.kernel, thread_id);
    return CALL_EXPORT(sceKernelSendMsgPipe, msgPipeId, pSendBuf, sendSize, waitMode, pResult, pTimeout);
}

EXPORT(int, sceKernelSendMsgPipeVector) {
    TRACY_FUNC(sceKernelSendMsgPipeVector);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSendMsgPipeVectorCB) {
    TRACY_FUNC(sceKernelSendMsgPipeVectorCB);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetEventWithNotifyCallback) {
    TRACY_FUNC(sceKernelSetEventWithNotifyCallback);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSetThreadContextForVM, SceUID threadId, Ptr<SceKernelThreadCpuRegisterInfo> pCpuRegisterInfo, Ptr<SceKernelThreadVfpRegisterInfo> pVfpRegisterInfo) {
    TRACY_FUNC(sceKernelSetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
    return CALL_EXPORT(_sceKernelSetThreadContextForVM, threadId, pCpuRegisterInfo, pVfpRegisterInfo);
}

EXPORT(int, sceKernelSetTimerEvent, SceUID timer_handle, SceInt32 type, SceKernelSysClock *interval, SceInt32 repeats) {
    TRACY_FUNC(sceKernelSetTimerEvent, timer_handle, type, interval, repeats);

    return timer_set(emuenv.kernel, export_name, thread_id, timer_handle, type, interval, repeats);
}

EXPORT(int, sceKernelSetTimerTime) {
    TRACY_FUNC(sceKernelSetTimerTime);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSignalLwCond, Ptr<SceKernelLwCondWork> workarea) {
    TRACY_FUNC(sceKernelSignalLwCond, workarea);
    SceUID condid = workarea.get(emuenv.mem)->uid;
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Any), SyncWeight::Light);
}

EXPORT(int, sceKernelSignalLwCondAll, Ptr<SceKernelLwCondWork> workarea) {
    TRACY_FUNC(sceKernelSignalLwCondAll, workarea);
    SceUID condid = workarea.get(emuenv.mem)->uid;
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::All), SyncWeight::Light);
}

EXPORT(int, sceKernelSignalLwCondTo, Ptr<SceKernelLwCondWork> workarea, SceUID thread_target) {
    TRACY_FUNC(sceKernelSignalLwCondTo, workarea, thread_target);
    SceUID condid = workarea.get(emuenv.mem)->uid;
    return condvar_signal(emuenv.kernel, export_name, thread_id, condid,
        Condvar::SignalTarget(Condvar::SignalTarget::Type::Specific, thread_target), SyncWeight::Light);
}

EXPORT(int, sceKernelStackChkFail) {
    TRACY_FUNC(sceKernelStackChkFail);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelStartModule, SceUID uid, SceSize args, const Ptr<void> argp, SceUInt32 flags, const Ptr<SceKernelStartModuleOpt> pOpt, int *pRes) {
    TRACY_FUNC(sceKernelStartModule, uid, args, argp, flags, pOpt, pRes);
    return CALL_EXPORT(_sceKernelStartModule, uid, args, argp, flags, pOpt, pRes);
}

EXPORT(int, sceKernelStartThread, SceUID thid, SceSize arglen, Ptr<void> argp) {
    TRACY_FUNC(sceKernelStartThread, thid, arglen, argp);
    return CALL_EXPORT(_sceKernelStartThread, thid, arglen, argp);
}

EXPORT(int, sceKernelStopModule) {
    TRACY_FUNC(sceKernelStopModule);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelStopUnloadModule) {
    TRACY_FUNC(sceKernelStopUnloadModule);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTryLockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int lock_count) {
    TRACY_FUNC(sceKernelTryLockLwMutex, workarea, lock_count);
    const auto lwmutexid = workarea.get(emuenv.mem)->uid;
    return mutex_try_lock(emuenv.kernel, emuenv.mem, export_name, thread_id, lwmutexid, lock_count, SyncWeight::Light);
}

EXPORT(int, sceKernelTryReceiveMsgPipe, SceUID msgpipe_id, char *recv_buf, SceSize msg_size, SceUInt32 wait_mode, SceSize *result) {
    TRACY_FUNC(sceKernelTryReceiveMsgPipe, msgpipe_id, recv_buf, msg_size, wait_mode, result);
    const auto ret = msgpipe_recv(emuenv.kernel, export_name, thread_id, msgpipe_id, wait_mode | SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT, recv_buf, msg_size, 0);
    if (ret == 0) {
        return SCE_KERNEL_ERROR_MPP_EMPTY;
    }
    if (result) {
        *result = ret;
    }
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelTryReceiveMsgPipeVector) {
    TRACY_FUNC(sceKernelTryReceiveMsgPipeVector);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTrySendMsgPipe, SceUID msgPipeId, const void *pSendBuf, SceSize sendSize, SceUInt32 waitMode, SceSize *pResult) {
    TRACY_FUNC(sceKernelTrySendMsgPipe, msgPipeId, pSendBuf, sendSize, waitMode, pResult);
    STUBBED("");
    waitMode |= SCE_KERNEL_MSG_PIPE_MODE_DONT_WAIT;
    SceUInt32 pTimeout = 0;
    const auto ret = msgpipe_send(emuenv.kernel, export_name, thread_id, msgPipeId, waitMode, pSendBuf, sendSize, &pTimeout);
    if (static_cast<int>(ret) < 0) {
        return ret;
    }
    if (pResult) {
        *pResult = ret;
    }
    return SCE_KERNEL_OK;
    // return UNIMPLEMENTED();
}

EXPORT(int, sceKernelTrySendMsgPipeVector) {
    TRACY_FUNC(sceKernelTrySendMsgPipeVector);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnloadModule) {
    TRACY_FUNC(sceKernelUnloadModule);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelUnlockLwMutex, Ptr<SceKernelLwMutexWork> workarea, int unlock_count) {
    TRACY_FUNC(sceKernelUnlockLwMutex, workarea, unlock_count);
    const auto lwmutexid = workarea.get(emuenv.mem)->uid;
    return mutex_unlock(emuenv.kernel, export_name, thread_id, lwmutexid, unlock_count, SyncWeight::Light);
}

EXPORT(int, sceKernelUnlockLwMutex_0, Ptr<SceKernelLwMutexWork> workarea, int unlock_count) {
    TRACY_FUNC(sceKernelUnlockLwMutex_0, workarea, unlock_count);
    return CALL_EXPORT(sceKernelUnlockLwMutex, workarea, unlock_count);
}

EXPORT(int, sceKernelUnlockLwMutex2, Ptr<SceKernelLwMutexWork> workarea, int unlock_count) {
    TRACY_FUNC(sceKernelUnlockLwMutex2, workarea, unlock_count);
    const auto lwmutexid = workarea.get(emuenv.mem)->uid;
    return mutex_unlock(emuenv.kernel, export_name, thread_id, lwmutexid, unlock_count, SyncWeight::Light);
}

EXPORT(SceInt32, sceKernelWaitCond, SceUID condId, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelWaitCond, condId, pTimeout);
    return CALL_EXPORT(_sceKernelWaitCond, condId, pTimeout);
}

EXPORT(SceInt32, sceKernelWaitCondCB, SceUID condId, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelWaitCondCB, condId, pTimeout);
    return CALL_EXPORT(_sceKernelWaitCondCB, condId, pTimeout);
}

EXPORT(SceInt32, sceKernelWaitEvent, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout) {
    TRACY_FUNC(sceKernelWaitEvent, event_id, bit_pattern, result_pattern, user_data, timeout);
    return CALL_EXPORT(_sceKernelWaitEvent, event_id, bit_pattern, result_pattern, user_data, timeout);
}

EXPORT(SceInt32, sceKernelWaitEventCB, SceUID event_id, SceUInt32 bit_pattern, SceUInt32 *result_pattern, SceUInt64 *user_data, SceUInt32 *timeout) {
    TRACY_FUNC(sceKernelWaitEventCB, event_id, bit_pattern, result_pattern, user_data, timeout);
    return CALL_EXPORT(_sceKernelWaitEventCB, event_id, bit_pattern, result_pattern, user_data, timeout);
}

EXPORT(SceInt32, sceKernelWaitEventFlag, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelWaitEventFlag, evfId, bitPattern, waitMode, pResultPat, pTimeout);
    return eventflag_wait(emuenv.kernel, export_name, thread_id, evfId, bitPattern, waitMode, pResultPat, pTimeout);
}

EXPORT(SceInt32, sceKernelWaitEventFlagCB, SceUID evfId, SceUInt32 bitPattern, SceUInt32 waitMode, SceUInt32 *pResultPat, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelWaitEventFlagCB, evfId, bitPattern, waitMode, pResultPat, pTimeout);
    return CALL_EXPORT(_sceKernelWaitEventFlagCB, evfId, bitPattern, waitMode, pResultPat, pTimeout);
}

EXPORT(int, sceKernelWaitException) {
    TRACY_FUNC(sceKernelWaitException);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitExceptionCB) {
    TRACY_FUNC(sceKernelWaitExceptionCB);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitLwCond, Ptr<SceKernelLwCondWork> workarea, SceUInt32 *timeout) {
    TRACY_FUNC(sceKernelWaitLwCond, workarea, timeout);
    const auto cond_id = workarea.get(emuenv.mem)->uid;
    return condvar_wait(emuenv.kernel, emuenv.mem, export_name, thread_id, cond_id, timeout, SyncWeight::Light);
}

EXPORT(SceInt32, sceKernelWaitLwCondCB, Ptr<SceKernelLwCondWork> pWork, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelWaitLwCondCB, pWork, pTimeout);
    return CALL_EXPORT(_sceKernelWaitLwCondCB, pWork, pTimeout);
}

EXPORT(int, sceKernelWaitMultipleEvents) {
    TRACY_FUNC(sceKernelWaitMultipleEvents);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelWaitMultipleEventsCB) {
    TRACY_FUNC(sceKernelWaitMultipleEventsCB);
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceKernelWaitSema, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelWaitSema, semaId, needCount, pTimeout);
    return CALL_EXPORT(_sceKernelWaitSema, semaId, needCount, pTimeout);
}

EXPORT(SceInt32, sceKernelWaitSemaCB, SceUID semaId, SceInt32 needCount, SceUInt32 *pTimeout) {
    TRACY_FUNC(sceKernelWaitSemaCB, semaId, needCount, pTimeout);
    return CALL_EXPORT(_sceKernelWaitSemaCB, semaId, needCount, pTimeout);
}

EXPORT(int, sceKernelWaitSignal, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    TRACY_FUNC(sceKernelWaitSignal, unknown, delay, timeout);
    return CALL_EXPORT(_sceKernelWaitSignal, unknown, delay, timeout);
}

EXPORT(int, sceKernelWaitSignalCB, uint32_t unknown, uint32_t delay, uint32_t timeout) {
    TRACY_FUNC(sceKernelWaitSignalCB, unknown, delay, timeout);
    return CALL_EXPORT(_sceKernelWaitSignalCB, unknown, delay, timeout);
}

EXPORT(int, sceKernelWaitThreadEnd, SceUID thid, int *stat, SceUInt *timeout) {
    TRACY_FUNC(sceKernelWaitThreadEnd, thid, stat, timeout);
    return CALL_EXPORT(_sceKernelWaitThreadEnd, thid, stat, timeout);
}

EXPORT(int, sceKernelWaitThreadEndCB, SceUID thid, int *stat, SceUInt *timeout) {
    TRACY_FUNC(sceKernelWaitThreadEndCB, thid, stat, timeout);
    return CALL_EXPORT(_sceKernelWaitThreadEndCB, thid, stat, timeout);
}

EXPORT(int, sceSblACMgrIsGameProgram) {
    TRACY_FUNC(sceSblACMgrIsGameProgram);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Auth1) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160Auth1);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Auth2) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160Auth2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Auth3) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160Auth3);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Auth4) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160Auth4);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Auth5) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160Auth5);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160BroadCastDecrypt) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160BroadCastDecrypt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160BroadCastEncrypt) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160BroadCastEncrypt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160GetKeys) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160GetKeys);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Init) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160Init);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160Shutdown) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160Shutdown);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160UniCastDecrypt) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160UniCastDecrypt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB160UniCastEncrypt) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB160UniCastEncrypt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Auth1) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB224Auth1);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Auth2) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB224Auth2);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Auth3) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB224Auth3);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Auth4) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB224Auth4);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Auth5) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB224Auth5);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224GetKeys) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB224GetKeys);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Init) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB224Init);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrAdhocBB224Shutdown) {
    TRACY_FUNC(sceSblGcAuthMgrAdhocBB224Shutdown);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrGetMediaIdType01) {
    TRACY_FUNC(sceSblGcAuthMgrGetMediaIdType01);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBCipherFinal) {
    TRACY_FUNC(sceSblGcAuthMgrMsSaveBBCipherFinal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBCipherInit) {
    TRACY_FUNC(sceSblGcAuthMgrMsSaveBBCipherInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBCipherUpdate) {
    TRACY_FUNC(sceSblGcAuthMgrMsSaveBBCipherUpdate);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBMacFinal) {
    TRACY_FUNC(sceSblGcAuthMgrMsSaveBBMacFinal);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBMacInit) {
    TRACY_FUNC(sceSblGcAuthMgrMsSaveBBMacInit);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrMsSaveBBMacUpdate) {
    TRACY_FUNC(sceSblGcAuthMgrMsSaveBBMacUpdate);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrPcactActivation) {
    TRACY_FUNC(sceSblGcAuthMgrPcactActivation);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrPcactGetChallenge) {
    TRACY_FUNC(sceSblGcAuthMgrPcactGetChallenge);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrPkgVry) {
    TRACY_FUNC(sceSblGcAuthMgrPkgVry);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrPsmactCreateC1) {
    TRACY_FUNC(sceSblGcAuthMgrPsmactCreateC1);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrPsmactVerifyR1) {
    TRACY_FUNC(sceSblGcAuthMgrPsmactVerifyR1);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrSclkGetData1) {
    TRACY_FUNC(sceSblGcAuthMgrSclkGetData1);
    return UNIMPLEMENTED();
}

EXPORT(int, sceSblGcAuthMgrSclkSetData2) {
    TRACY_FUNC(sceSblGcAuthMgrSclkSetData2);
    return UNIMPLEMENTED();
}

VAR_BRIDGE_IMPL(__stack_chk_guard)
VAR_BRIDGE_IMPL(__sce_libcparam)
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
BRIDGE_IMPL(sceKernelLockLwMutex_0)
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
BRIDGE_IMPL(sceKernelUnlockLwMutex_0)
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
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160Auth1)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160Auth2)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160Auth3)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160Auth4)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160Auth5)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160BroadCastDecrypt)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160BroadCastEncrypt)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160GetKeys)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160Init)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160Shutdown)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160UniCastDecrypt)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB160UniCastEncrypt)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224Auth1)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224Auth2)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224Auth3)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224Auth4)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224Auth5)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224GetKeys)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224Init)
BRIDGE_IMPL(sceSblGcAuthMgrAdhocBB224Shutdown)
BRIDGE_IMPL(sceSblGcAuthMgrGetMediaIdType01)
BRIDGE_IMPL(sceSblGcAuthMgrMsSaveBBCipherFinal)
BRIDGE_IMPL(sceSblGcAuthMgrMsSaveBBCipherInit)
BRIDGE_IMPL(sceSblGcAuthMgrMsSaveBBCipherUpdate)
BRIDGE_IMPL(sceSblGcAuthMgrMsSaveBBMacFinal)
BRIDGE_IMPL(sceSblGcAuthMgrMsSaveBBMacInit)
BRIDGE_IMPL(sceSblGcAuthMgrMsSaveBBMacUpdate)
BRIDGE_IMPL(sceSblGcAuthMgrPcactActivation)
BRIDGE_IMPL(sceSblGcAuthMgrPcactGetChallenge)
BRIDGE_IMPL(sceSblGcAuthMgrPkgVry)
BRIDGE_IMPL(sceSblGcAuthMgrPsmactCreateC1)
BRIDGE_IMPL(sceSblGcAuthMgrPsmactVerifyR1)
BRIDGE_IMPL(sceSblGcAuthMgrSclkGetData1)
BRIDGE_IMPL(sceSblGcAuthMgrSclkSetData2)
