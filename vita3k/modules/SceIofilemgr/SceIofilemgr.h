// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

DECL_EXPORT(int, _sceIoDopen, const char *dir);
DECL_EXPORT(int, _sceIoDread, const SceUID fd, SceIoDirent *dir);
DECL_EXPORT(int, _sceIoMkdir, const char *dir, const SceMode mode);
DECL_EXPORT(SceOff, _sceIoLseek, const SceUID fd, Ptr<_sceIoLseekOpt> opt);
DECL_EXPORT(int, _sceIoGetstat, const char *file, SceIoStat *stat);
