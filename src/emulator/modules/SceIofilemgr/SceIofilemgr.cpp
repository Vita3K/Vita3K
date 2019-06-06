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

#include "SceIofilemgr.h"

#include <io/functions.h>

EXPORT(int, _sceIoChstat) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoChstatAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoChstatByFd) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoCompleteMultiple) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoDevctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoDevctlAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoDopen) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoDread) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoGetstat) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoGetstatAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoGetstatByFd) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoIoctl) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoIoctlAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoLseek) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoLseekAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoMkdir) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoMkdirAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoOpen) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoOpenAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoPread) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoPreadAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoPwrite) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoPwriteAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRemove) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRemoveAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRename) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRenameAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRmdir) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoRmdirAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoSync) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceIoSyncAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoCancel) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoChstatByFdAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoClose, SceUID fd) {
    return close_file(host.io, fd, export_name);
}

EXPORT(int, sceIoCloseAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoComplete) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDclose, SceUID fd) {
    return close_dir(host.io, fd, export_name);
}

EXPORT(int, sceIoDcloseAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDopenAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoDreadAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoFlockForSystem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetPriorityForSystem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetProcessDefaultPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetThreadDefaultPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetThreadDefaultPriorityForSystem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetstatByFdAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoLseek32, SceUID fd, int offset, int whence) {
    return seek_file(fd, offset, whence, host.io, export_name);
}

EXPORT(int, sceIoRead, SceUID fd, void *data, SceSize size) {
    return read_file(data, host.io, fd, size, export_name);
}

EXPORT(int, sceIoReadAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetPriorityForSystem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetProcessDefaultPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetThreadDefaultPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSetThreadDefaultPriorityForSystem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSyncByFd) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoSyncByFdAsync) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoWrite, SceUID fd, const void *data, SceSize size) {
    return write_file(fd, data, size, host.io, export_name);
}

EXPORT(int, sceIoWriteAsync) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceIoChstat)
BRIDGE_IMPL(_sceIoChstatAsync)
BRIDGE_IMPL(_sceIoChstatByFd)
BRIDGE_IMPL(_sceIoCompleteMultiple)
BRIDGE_IMPL(_sceIoDevctl)
BRIDGE_IMPL(_sceIoDevctlAsync)
BRIDGE_IMPL(_sceIoDopen)
BRIDGE_IMPL(_sceIoDread)
BRIDGE_IMPL(_sceIoGetstat)
BRIDGE_IMPL(_sceIoGetstatAsync)
BRIDGE_IMPL(_sceIoGetstatByFd)
BRIDGE_IMPL(_sceIoIoctl)
BRIDGE_IMPL(_sceIoIoctlAsync)
BRIDGE_IMPL(_sceIoLseek)
BRIDGE_IMPL(_sceIoLseekAsync)
BRIDGE_IMPL(_sceIoMkdir)
BRIDGE_IMPL(_sceIoMkdirAsync)
BRIDGE_IMPL(_sceIoOpen)
BRIDGE_IMPL(_sceIoOpenAsync)
BRIDGE_IMPL(_sceIoPread)
BRIDGE_IMPL(_sceIoPreadAsync)
BRIDGE_IMPL(_sceIoPwrite)
BRIDGE_IMPL(_sceIoPwriteAsync)
BRIDGE_IMPL(_sceIoRemove)
BRIDGE_IMPL(_sceIoRemoveAsync)
BRIDGE_IMPL(_sceIoRename)
BRIDGE_IMPL(_sceIoRenameAsync)
BRIDGE_IMPL(_sceIoRmdir)
BRIDGE_IMPL(_sceIoRmdirAsync)
BRIDGE_IMPL(_sceIoSync)
BRIDGE_IMPL(_sceIoSyncAsync)
BRIDGE_IMPL(sceIoCancel)
BRIDGE_IMPL(sceIoChstatByFdAsync)
BRIDGE_IMPL(sceIoClose)
BRIDGE_IMPL(sceIoCloseAsync)
BRIDGE_IMPL(sceIoComplete)
BRIDGE_IMPL(sceIoDclose)
BRIDGE_IMPL(sceIoDcloseAsync)
BRIDGE_IMPL(sceIoDopenAsync)
BRIDGE_IMPL(sceIoDreadAsync)
BRIDGE_IMPL(sceIoFlockForSystem)
BRIDGE_IMPL(sceIoGetPriority)
BRIDGE_IMPL(sceIoGetPriorityForSystem)
BRIDGE_IMPL(sceIoGetProcessDefaultPriority)
BRIDGE_IMPL(sceIoGetThreadDefaultPriority)
BRIDGE_IMPL(sceIoGetThreadDefaultPriorityForSystem)
BRIDGE_IMPL(sceIoGetstatByFdAsync)
BRIDGE_IMPL(sceIoLseek32)
BRIDGE_IMPL(sceIoRead)
BRIDGE_IMPL(sceIoReadAsync)
BRIDGE_IMPL(sceIoSetPriority)
BRIDGE_IMPL(sceIoSetPriorityForSystem)
BRIDGE_IMPL(sceIoSetProcessDefaultPriority)
BRIDGE_IMPL(sceIoSetThreadDefaultPriority)
BRIDGE_IMPL(sceIoSetThreadDefaultPriorityForSystem)
BRIDGE_IMPL(sceIoSyncByFd)
BRIDGE_IMPL(sceIoSyncByFdAsync)
BRIDGE_IMPL(sceIoWrite)
BRIDGE_IMPL(sceIoWriteAsync)
