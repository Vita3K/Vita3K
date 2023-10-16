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

#include "SceIofilemgr.h"

#include <io/functions.h>
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

EXPORT(int, _sceIoDevctl) {
    TRACY_FUNC(_sceIoDevctl);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoDevctlAsync) {
    TRACY_FUNC(_sceIoDevctlAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoDopen, const char *dir) {
    TRACY_FUNC(_sceIoDopen, dir);
    return open_dir(emuenv.io, dir, emuenv.pref_path.wstring(), export_name);
}

EXPORT(int, _sceIoDread, const SceUID fd, SceIoDirent *dir) {
    TRACY_FUNC(_sceIoDread, fd, dir);
    if (dir == nullptr) {
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    }
    return read_dir(emuenv.io, fd, dir, emuenv.pref_path.wstring(), export_name);
}

EXPORT(int, _sceIoGetstat, const char *file, SceIoStat *stat) {
    TRACY_FUNC(_sceIoGetstat, file, stat);
    return stat_file(emuenv.io, file, stat, emuenv.pref_path.wstring(), export_name);
}

EXPORT(int, _sceIoGetstatAsync) {
    TRACY_FUNC(_sceIoGetstatAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoGetstatByFd, const SceUID fd, SceIoStat *stat) {
    TRACY_FUNC(_sceIoGetstatByFd, fd, stat);
    return stat_file_by_fd(emuenv.io, fd, stat, emuenv.pref_path.wstring(), export_name);
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
    return create_dir(emuenv.io, dir, mode, emuenv.pref_path.wstring(), export_name);
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
    return open_file(emuenv.io, file, flags, emuenv.pref_path.wstring(), export_name);
}

EXPORT(int, _sceIoOpenAsync) {
    TRACY_FUNC(_sceIoOpenAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoPread) {
    TRACY_FUNC(_sceIoPread);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoPreadAsync) {
    TRACY_FUNC(_sceIoPreadAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoPwrite) {
    TRACY_FUNC(_sceIoPwrite);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoPwriteAsync) {
    TRACY_FUNC(_sceIoPwriteAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRemove) {
    TRACY_FUNC(_sceIoRemove);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRemoveAsync) {
    TRACY_FUNC(_sceIoRemoveAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRename) {
    TRACY_FUNC(_sceIoRename);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRenameAsync) {
    TRACY_FUNC(_sceIoRenameAsync);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRmdir) {
    TRACY_FUNC(_sceIoRmdir);
    return UNIMPLEMENTED();
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
