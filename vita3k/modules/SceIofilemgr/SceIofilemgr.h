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

#pragma once

#include <io/types.h>
#include <module/module.h>

typedef struct _sceIoLseekOpt {
    SceOff offset;
    SceIoSeekMode whence;
    uint32_t unk;
} _sceIoLseekOpt;

// A 64 bit file offset as libkernel passes it to the pread and pwrite syscalls, low word first
struct _sceIoOffsetOpt {
    SceUInt32 low;
    SceUInt32 high;
};

// The input length, output buffer and output length of a devctl or ioctl syscall
struct _sceIoControlOpt {
    SceSize inlen;
    Ptr<void> outdata;
    SceSize outlen;
};

// The devctl and dread behind both the user exports and their syscalls
// The host pointer of a guest buffer of size bytes, or nullptr when it isn't all mapped
template <typename T>
T *mapped_buffer(const MemState &mem, Ptr<T> buf, SceSize size) {
    return is_valid_addr_len(mem, buf.address(), size) ? buf.get(mem) : nullptr;
}

int devctl(EmuEnvState &emuenv, const char *export_name, const char *dev, SceInt cmd, Ptr<const void> indata, SceSize inlen, Ptr<void> outdata, SceSize outlen);
int dread(EmuEnvState &emuenv, const char *export_name, SceUID fd, SceIoDirent *dir);
int ioctl(EmuEnvState &emuenv, const char *export_name, SceUID fd, int cmd, Ptr<const void> indata, SceSize inlen, Ptr<void> outdata, SceSize outlen);
