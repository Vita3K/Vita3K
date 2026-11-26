// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "SceIofilemgr.h"

#include <io/device.h>
#include <io/functions.h>
#include <io/io.h>
#include <kernel/types.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceIofilemgr);

EXPORT(int, _sceIoChstat) {
    TRACY_FUNC(_sceIoChstat);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoChstatAsync) {
    TRACY_FUNC(_sceIoChstatAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoChstatByFd) {
    TRACY_FUNC(_sceIoChstatByFd);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoCompleteMultiple) {
    TRACY_FUNC(_sceIoCompleteMultiple);
    return UNIMPLEMENTED();
}

int devctl(EmuEnvState &emuenv, const char *export_name, const char *dev, SceInt cmd, Ptr<const void> indata, SceSize inlen, Ptr<void> outdata, SceSize outlen) {
    // The device name is copied with a limit of 0x400 bytes
    if (dev && strnlen(dev, 0x400) == 0x400)
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    // Commands with these bits are for system programs
    if (cmd & 0xE00)
        return RET_ERROR(SCE_ERROR_ERRNO_EUNSUP);
    if (indata && !is_valid_addr_len(emuenv.mem, indata.address(), inlen))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    if (!dev)
        return RET_ERROR(SCE_ERROR_ERRNO_EFAULT);
    // Only the root of a device takes commands
    const VitaIoDevice device = device::get_device(dev);
    if (device == VitaIoDevice::_INVALID || !device::remove_device_from_path(dev, device).empty())
        return RET_ERROR(SCE_ERROR_ERRNO_ENODEV);
    switch (cmd) {
    case SCE_IO_DEVCTL_GET_FREE_SPACE: {
        // The short answer ends before the volume fields
        if (outlen != offsetof(SceIoDevInfo, serial) && outlen != sizeof(SceIoDevInfo))
            return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);

        // A device with no host directory has no volume to report on
        std::error_code error;
        const fs::space_info space = fs::space(emuenv.vita_fs_path / device::get_device_string(device, false), error);
        if (error)
            return RET_ERROR(SCE_ERROR_ERRNO_ENODEV);
        // The devices are host directories with no cluster size, so the answer uses a fixed one that
        // the sizes are rounded to
        constexpr SceSize cluster_size = 4096;
        // The answer is built in a buffer filled with 0xFF, which is copied whole
        SceIoDevInfo info;
        std::memset(&info, 0xFF, sizeof(info));
        info.max_size = space.capacity / cluster_size * cluster_size;
        info.free_size = space.available / cluster_size * cluster_size;
        info.cluster_size = cluster_size;
        if (outlen == sizeof(SceIoDevInfo)) {
            info.unused = 0;
            info.serial = 0;
            std::ranges::fill(info.label, ' ');
            info.end = 0;
        }
        if (outdata) {
            if (!is_valid_addr_len(emuenv.mem, outdata.address(), outlen))
                return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
            std::memcpy(outdata.get(emuenv.mem), &info, outlen);
        }
        return 0;
    }
    default:
        // The free space query is the only command a game can use
        LOG_WARN("{}: devctl command {:#x} on {} refused", export_name, cmd, dev);
        return RET_ERROR(SCE_ERROR_ERRNO_EUNSUP);
    }
}

int dread(EmuEnvState &emuenv, const char *export_name, SceUID fd, SceIoDirent *dir) {
    if (!dir)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    return read_dir(emuenv.io, fd, dir, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoDevctl, const char *dev, SceInt cmd, Ptr<const void> indata, Ptr<const _sceIoControlOpt> opt) {
    TRACY_FUNC(_sceIoDevctl, dev, cmd, indata, opt);
    // The syscall copies 0x18 bytes of the block
    if (!is_valid_addr_len(emuenv.mem, opt.address(), 0x18))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    const _sceIoControlOpt *const block = opt.get(emuenv.mem);
    return devctl(emuenv, export_name, dev, cmd, indata, block->inlen, block->outdata, block->outlen);
}

EXPORT(int, _sceIoDevctlAsync) {
    TRACY_FUNC(_sceIoDevctlAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoDopen, const char *dir) {
    TRACY_FUNC(_sceIoDopen, dir);
    return open_dir(emuenv.io, dir, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoDread, const SceUID fd, SceIoDirent *dir) {
    TRACY_FUNC(_sceIoDread, fd, dir);
    return dread(emuenv, export_name, fd, dir);
}

EXPORT(int, _sceIoGetstat, const char *file, SceIoStat *stat) {
    TRACY_FUNC(_sceIoGetstat, file, stat);
    return stat_file(emuenv.io, file, stat, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoGetstatAsync) {
    TRACY_FUNC(_sceIoGetstatAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoGetstatByFd, const SceUID fd, SceIoStat *stat) {
    TRACY_FUNC(_sceIoGetstatByFd, fd, stat);
    return stat_file_by_fd(emuenv.io, fd, stat, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoIoctl) {
    TRACY_FUNC(_sceIoIoctl);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoIoctlAsync) {
    TRACY_FUNC(_sceIoIoctlAsync);
    return UNIMPLEMENTED();
}

EXPORT(SceOff, _sceIoLseek, const SceUID fd, Ptr<_sceIoLseekOpt> opt) {
    TRACY_FUNC(_sceIoLseek, fd, opt);
    return seek_file(fd, opt.get(emuenv.mem)->offset, opt.get(emuenv.mem)->whence, emuenv.io, export_name);
}

EXPORT(int, _sceIoLseekAsync) {
    TRACY_FUNC(_sceIoLseekAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoMkdir, const char *dir, const SceMode mode) {
    TRACY_FUNC(_sceIoMkdir, dir, mode);
    return create_dir(emuenv.io, dir, mode, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoMkdirAsync) {
    TRACY_FUNC(_sceIoMkdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoOpen, const char *file, const int flags, const SceMode mode) {
    TRACY_FUNC(_sceIoOpen, file, flags, mode);
    if (file == nullptr) {
        return RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
    }
    LOG_INFO("Opening file: {}", file);
    return open_file(emuenv.io, file, flags, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoOpenAsync) {
    TRACY_FUNC(_sceIoOpenAsync);
    return UNIMPLEMENTED();
}

EXPORT(SceSSize, _sceIoPread, SceUID fd, Ptr<void> buf, SceSize size, Ptr<const _sceIoOffsetOpt> offset) {
    TRACY_FUNC(_sceIoPread, fd, buf, size, offset);
    // The syscall copies 0x10 bytes of the block
    if (!is_valid_addr_len(emuenv.mem, offset.address(), 0x10))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    const _sceIoOffsetOpt *const block = offset.get(emuenv.mem);
    return pread_file(emuenv.io, fd, mapped_buffer(emuenv.mem, buf, size), size, (static_cast<SceOff>(block->high) << 32) | block->low, export_name);
}

EXPORT(int, _sceIoPreadAsync) {
    TRACY_FUNC(_sceIoPreadAsync);
    return UNIMPLEMENTED();
}

EXPORT(SceSSize, _sceIoPwrite, SceUID fd, Ptr<const void> buf, SceSize size, Ptr<const _sceIoOffsetOpt> offset) {
    TRACY_FUNC(_sceIoPwrite, fd, buf, size, offset);
    // The syscall copies 0x10 bytes of the block
    if (!is_valid_addr_len(emuenv.mem, offset.address(), 0x10))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    const _sceIoOffsetOpt *const block = offset.get(emuenv.mem);
    return pwrite_file(emuenv.io, fd, mapped_buffer(emuenv.mem, buf, size), size, (static_cast<SceOff>(block->high) << 32) | block->low, export_name);
}

EXPORT(int, _sceIoPwriteAsync) {
    TRACY_FUNC(_sceIoPwriteAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRemove, const char *path) {
    TRACY_FUNC(_sceIoRemove, path);
    return remove_file(emuenv.io, path, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoRemoveAsync) {
    TRACY_FUNC(_sceIoRemoveAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRename, const char *old_name, const char *new_name) {
    TRACY_FUNC(_sceIoRename, old_name, new_name);
    return rename(emuenv.io, old_name, new_name, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoRenameAsync) {
    TRACY_FUNC(_sceIoRenameAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRmdir, const char *path) {
    TRACY_FUNC(_sceIoRmdir, path);
    return remove_dir(emuenv.io, path, emuenv.vita_fs_path, export_name);
}

EXPORT(int, _sceIoRmdirAsync) {
    TRACY_FUNC(_sceIoRmdirAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoSync) {
    TRACY_FUNC(_sceIoSync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoSyncAsync) {
    TRACY_FUNC(_sceIoSyncAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoCancel) {
    TRACY_FUNC(sceIoCancel);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoChstatByFdAsync) {
    TRACY_FUNC(sceIoChstatByFdAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoClose, const SceUID fd) {
    TRACY_FUNC(sceIoClose, fd);
    return close_file(emuenv.io, fd, export_name);
}

EXPORT(int, sceIoCloseAsync) {
    TRACY_FUNC(sceIoCloseAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoComplete) {
    TRACY_FUNC(sceIoComplete);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDclose, const SceUID fd) {
    TRACY_FUNC(sceIoDclose, fd);
    return close_dir(emuenv.io, fd, export_name);
}

EXPORT(int, sceIoDcloseAsync) {
    TRACY_FUNC(sceIoDcloseAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDopenAsync) {
    TRACY_FUNC(sceIoDopenAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDreadAsync) {
    TRACY_FUNC(sceIoDreadAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoFlockForSystem) {
    TRACY_FUNC(sceIoFlockForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetPriority) {
    TRACY_FUNC(sceIoGetPriority);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetPriorityForSystem) {
    TRACY_FUNC(sceIoGetPriorityForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetProcessDefaultPriority) {
    TRACY_FUNC(sceIoGetProcessDefaultPriority);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetThreadDefaultPriority) {
    TRACY_FUNC(sceIoGetThreadDefaultPriority);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetThreadDefaultPriorityForSystem) {
    TRACY_FUNC(sceIoGetThreadDefaultPriorityForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetstatByFdAsync) {
    TRACY_FUNC(sceIoGetstatByFdAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoLseek32, const SceUID fd, const int32_t offset, const SceIoSeekMode whence) {
    TRACY_FUNC(sceIoLseek32, fd, offset, whence);
    return static_cast<int>(seek_file(fd, offset, whence, emuenv.io, export_name));
}

EXPORT(int, sceIoRead, const SceUID fd, void *data, const SceSize size) {
    TRACY_FUNC(sceIoRead, fd, data, size);
    return read_file(data, emuenv.io, fd, size, export_name);
}

EXPORT(int, sceIoReadAsync) {
    TRACY_FUNC(sceIoReadAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetPriority) {
    TRACY_FUNC(sceIoSetPriority);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetPriorityForSystem) {
    TRACY_FUNC(sceIoSetPriorityForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetProcessDefaultPriority) {
    TRACY_FUNC(sceIoSetProcessDefaultPriority);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetThreadDefaultPriority) {
    TRACY_FUNC(sceIoSetThreadDefaultPriority);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetThreadDefaultPriorityForSystem) {
    TRACY_FUNC(sceIoSetThreadDefaultPriorityForSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSyncByFd) {
    TRACY_FUNC(sceIoSyncByFd);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSyncByFdAsync) {
    TRACY_FUNC(sceIoSyncByFdAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoWrite, const SceUID fd, const void *data, const SceSize size) {
    TRACY_FUNC(sceIoWrite, fd, data, size);
    return write_file(fd, data, size, emuenv.io, export_name);
}

EXPORT(int, sceIoWriteAsync) {
    TRACY_FUNC(sceIoWriteAsync);
    return UNIMPLEMENTED();
}
