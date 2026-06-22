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

#include <kernel/types.h>
#include <module/module.h>

struct SceKernelAllocMemBlockKernelOpt {
    SceSize size;
    SceUInt32 field_4;
    SceUInt32 attr;
    SceUInt32 field_C; // vbase when attr & 0x1
    SceUInt32 paddr;
    SceSize alignment;
    SceUInt32 extraLow;
    SceUInt32 extraHigh;
    SceUInt32 mirror_blockid;
    SceUID pid;
    uint32_t *paddr_list;
    SceUInt32 field_2C;
    SceUInt32 field_30;
    SceUInt32 field_34;
    SceUInt32 field_38;
    SceUInt32 field_3C;
    SceUInt32 field_40;
    SceUInt32 field_44;
    SceUInt32 field_48;
    SceUInt32 field_4C;
    SceUInt32 field_50;
    SceUInt32 field_54;
};

constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_VBASE = 0x1;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT = 4;

DECL_EXPORT(SceUID, ksceKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockKernelOpt *opt);
DECL_EXPORT(int, ksceKernelGetMemBlockBase, SceUID uid, Ptr<void> *basep);
DECL_EXPORT(int, ksceKernelFreeMemBlock, SceUID uid);
