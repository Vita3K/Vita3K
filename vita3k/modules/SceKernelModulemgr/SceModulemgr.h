// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

EXPORT(SceUID, _sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option);
EXPORT(SceUID, _sceKernelLoadStartModule, const char *moduleFileName, SceSize args, const Ptr<void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes);
EXPORT(int, _sceKernelStartModule, SceUID uid, SceSize args, const Ptr<void> argp, SceUInt32 flags, const Ptr<SceKernelStartModuleOpt> pOpt, int *pRes);

BRIDGE_DECL(_sceKernelCloseModule)
BRIDGE_DECL(_sceKernelLoadModule)
BRIDGE_DECL(_sceKernelLoadStartModule)
BRIDGE_DECL(_sceKernelOpenModule)
BRIDGE_DECL(_sceKernelStartModule)
BRIDGE_DECL(_sceKernelStopModule)
BRIDGE_DECL(_sceKernelStopUnloadModule)
BRIDGE_DECL(_sceKernelUnloadModule)
BRIDGE_DECL(sceKernelGetAllowedSdkVersionOnSystem)
BRIDGE_DECL(sceKernelGetLibraryInfoByNID)
BRIDGE_DECL(sceKernelGetModuleIdByAddr)
BRIDGE_DECL(sceKernelGetModuleInfo)
BRIDGE_DECL(sceKernelGetModuleList)
BRIDGE_DECL(sceKernelGetSystemSwVersion)
BRIDGE_DECL(sceKernelInhibitLoadingModule)
BRIDGE_DECL(sceKernelIsCalledFromSysModule)
