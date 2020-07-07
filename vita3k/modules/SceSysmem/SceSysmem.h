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

#pragma once

#include <module/module.h>

// TODO use macro
EXPORT(SceUID, sceKernelFindMemBlockByAddr, Address addr, uint32_t size);
EXPORT(int, sceKernelFreeMemBlock, SceUID uid);

BRIDGE_DECL(sceKernelAllocMemBlock)
BRIDGE_DECL(sceKernelAllocMemBlockForVM)
BRIDGE_DECL(sceKernelAllocUnmapMemBlock)
BRIDGE_DECL(sceKernelCheckModelCapability)
BRIDGE_DECL(sceKernelCloseMemBlock)
BRIDGE_DECL(sceKernelCloseVMDomain)
BRIDGE_DECL(sceKernelFindMemBlockByAddr)
BRIDGE_DECL(sceKernelFreeMemBlock)
BRIDGE_DECL(sceKernelFreeMemBlockForVM)
BRIDGE_DECL(sceKernelGetFreeMemorySize)
BRIDGE_DECL(sceKernelGetMemBlockBase)
BRIDGE_DECL(sceKernelGetMemBlockInfoByAddr)
BRIDGE_DECL(sceKernelGetMemBlockInfoByRange)
BRIDGE_DECL(sceKernelGetModel)
BRIDGE_DECL(sceKernelGetModelForCDialog)
BRIDGE_DECL(sceKernelGetSubbudgetInfo)
BRIDGE_DECL(sceKernelIsPSVitaTV)
BRIDGE_DECL(sceKernelOpenMemBlock)
BRIDGE_DECL(sceKernelOpenVMDomain)
BRIDGE_DECL(sceKernelSyncVMDomain)
