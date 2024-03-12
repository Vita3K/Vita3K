// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <module/module.h>

#include <kernel/types.h>

DECL_EXPORT(SceUID, _sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option);
DECL_EXPORT(SceUID, _sceKernelLoadStartModule, const char *moduleFileName, SceSize args, const Ptr<const void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes);
DECL_EXPORT(int, _sceKernelStartModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelStartModuleOpt *pOpt, int *pRes);
DECL_EXPORT(int, _sceKernelStopModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelStopModuleOpt *pOpt, int *pRes);
DECL_EXPORT(int, _sceKernelStopUnloadModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const void *pOpt, int *pRes);
DECL_EXPORT(int, _sceKernelUnloadModule, SceUID uid, SceUInt32 flags, const void *pOpt);
