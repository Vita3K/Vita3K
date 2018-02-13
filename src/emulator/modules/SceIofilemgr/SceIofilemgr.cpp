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
    return unimplemented("sceIoCancel");
}

EXPORT(int, sceIoClose, SceUID fd) {
    close_file(host.io, fd);
    return 0;
}

EXPORT(int, sceIoCloseAsync) {
    return unimplemented("sceIoCloseAsync");
}

EXPORT(int, sceIoComplete) {
    return unimplemented("sceIoComplete");
}

EXPORT(int, sceIoDclose, SceUID fd) {
    return close_dir(host.io, fd);
}

EXPORT(int, sceIoFlockForSystem) {
    return unimplemented("sceIoFlockForSystem");
}

EXPORT(int, sceIoGetPriority) {
    return unimplemented("sceIoGetPriority");
}

EXPORT(int, sceIoGetProcessDefaultPriority) {
    return unimplemented("sceIoGetProcessDefaultPriority");
}

EXPORT(int, sceIoGetThreadDefaultPriority) {
    return unimplemented("sceIoGetThreadDefaultPriority");
}

EXPORT(int, sceIoLseek32, SceUID fd, int offset, int whence) {
    return seek_file(fd, offset, whence, host.io);
}

EXPORT(int, sceIoRead, SceUID fd, void *data, SceSize size) {
    return read_file(data, host.io, fd, size);
}

EXPORT(int, sceIoReadAsync) {
    return unimplemented("sceIoReadAsync");
}

EXPORT(int, sceIoSetPriority) {
    return unimplemented("sceIoSetPriority");
}

EXPORT(int, sceIoSetPriorityForSystem) {
    return unimplemented("sceIoSetPriorityForSystem");
}

EXPORT(int, sceIoSetProcessDefaultPriority) {
    return unimplemented("sceIoSetProcessDefaultPriority");
}

EXPORT(int, sceIoSetThreadDefaultPriority) {
    return unimplemented("sceIoSetThreadDefaultPriority");
}

EXPORT(int, sceIoSetThreadDefaultPriorityForSystem) {
    return unimplemented("sceIoSetThreadDefaultPriorityForSystem");
}

EXPORT(int, sceIoSyncByFd) {
    return unimplemented("sceIoSyncByFd");
}

EXPORT(int, sceIoSyncByFdAsync) {
    return unimplemented("sceIoSyncByFdAsync");
}

EXPORT(int, sceIoWrite, SceUID fd, const void *data, SceSize size) {
    return write_file(fd, data, size, host.io);
}

EXPORT(int, sceIoWriteAsync) {
    return unimplemented("sceIoWriteAsync");
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
