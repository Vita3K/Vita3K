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

EXPORT(int, sceIoCancel) {
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

EXPORT(int, sceIoFlockForSystem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetProcessDefaultPriority) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceIoGetThreadDefaultPriority) {
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

BRIDGE_IMPL(sceIoCancel)
BRIDGE_IMPL(sceIoClose)
BRIDGE_IMPL(sceIoCloseAsync)
BRIDGE_IMPL(sceIoComplete)
BRIDGE_IMPL(sceIoDclose)
BRIDGE_IMPL(sceIoFlockForSystem)
BRIDGE_IMPL(sceIoGetPriority)
BRIDGE_IMPL(sceIoGetProcessDefaultPriority)
BRIDGE_IMPL(sceIoGetThreadDefaultPriority)
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
