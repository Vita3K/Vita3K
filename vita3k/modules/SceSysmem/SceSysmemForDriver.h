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

struct SceKernelAllocMemBlockOptKernel {
    SceSize size;
    SceUInt32 unk_4;
    SceUInt32 attr;
    // The address to map the block at
    Ptr<void> vbase;
    // The physical address to use as its base
    SceUInt32 pbase;
    SceSize alignment;
    // The sizes of the extra areas from vbase and after the block
    SceSize extraLow;
    SceSize extraHigh;
    // The block this one is based on
    SceUID baseMemBlock;
    SceUID pid;
    // A SceKernelPAVector
    Ptr<void> pPAV;
    // What the block's size is rounded up to
    SceSize roundupUnitSize;
    SceUInt8 domain;
    // Related to SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_0x20
    SceUInt32 unk_34[9];
};
static_assert(sizeof(SceKernelAllocMemBlockOptKernel) == 0x58);

// Attributes of an allocation, most of which enable a member of its option
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_VBASE = 0x00000001;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PBASE = 0x00000002;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT = 0x00000004;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_EXTRA_LOW = 0x00000008;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_EXTRA_HIGH = 0x00000010;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_0x20 = 0x00000020;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_BASE = 0x00000040;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID = 0x00000080;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_BASENAME = 0x00000200;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PPAV = 0x00001000;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ROUNDUP = 0x00002000;
// sceKernelOpenMemBlock may open the block
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_ENABLE_OPEN = 0x00004000;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_DOMAIN = 0x00010000;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_NOPHYPAGE = 0x00020000;
// Don't map the block
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_DONT_MAP = 0x00040000;
// The block is physically contiguous
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT = 0x00200000;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_UNK = 0x00400000;
// The block shares its base block's address or physical pages, with HAS_BASE
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_SHARE_VBASE = 0x00800000;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_SHARE_PHYPAGE = 0x01000000;
constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_ALLOW_PARTIAL_OP = 0x04000000;

// Fields of a memory block type code
// Set for a kernel type
constexpr SceUInt32 MEMBLOCK_TYPE_KERNEL_MASK = 0xF0000000;
// The partition index, which picks the user address range
constexpr SceUInt32 MEMBLOCK_TYPE_PARTITION_INDEX_MASK = 0x0F000000;
// The ScePhyMemPart its physical memory comes from
constexpr SceUInt32 MEMBLOCK_TYPE_PHYMEMPART_MASK = 0x00F00000;
// The memory attributes of its pages, from the ARM TEX, C and B bits, which the info calls report the
// SCE_KERNEL_MEMORY_TYPE_NORMAL bits of as the memory type
constexpr SceUInt32 MEMBLOCK_TYPE_MEMORY_ATTR_MASK = 0x0000FF00;
constexpr SceUInt32 MEMBLOCK_TYPE_MEMORY_ATTR_SHIFT = 8;
// What an app may do with it, which the info calls report as the access
constexpr SceUInt32 MEMBLOCK_TYPE_USER_ACCESS_MASK = 0x000000F0;
constexpr SceUInt32 MEMBLOCK_TYPE_USER_ACCESS_SHIFT = 4;

// The access bits of a block, which the info calls report without execute
constexpr SceUInt32 SCE_KERNEL_MEMORY_ACCESS_R = 0x04;
constexpr SceUInt32 SCE_KERNEL_MEMORY_ACCESS_W = 0x02;
constexpr SceUInt32 SCE_KERNEL_MEMORY_ACCESS_X = 0x01;

constexpr SceUInt32 MEMBLOCK_TYPE_PARTITION_INDEX_USER_CDRAM = 0x09000000;
constexpr SceUInt32 MEMBLOCK_TYPE_PARTITION_INDEX_USER_CDIALOG = 0x0E000000;
constexpr SceUInt32 MEMBLOCK_TYPE_PHYMEMPART_CDRAM = 0x00400000;
constexpr SceUInt32 MEMBLOCK_TYPE_PHYMEMPART_PHYCONT = 0x00800000;
constexpr SceUInt32 MEMBLOCK_TYPE_PHYMEMPART_CDIALOG = 0x00A00000;
// Sharable device memory, which the info calls report as memory type 0
constexpr SceUInt32 MEMBLOCK_TYPE_MEMORY_ATTR_DEVICE = 0x00000800;
constexpr SceUInt32 MEMBLOCK_TYPE_MEMORY_ATTR_NORMAL_NC = 0x00008000;

// The memory types the info calls report
constexpr SceUInt32 SCE_KERNEL_MEMORY_TYPE_NORMAL_NC = 0x80;
constexpr SceUInt32 SCE_KERNEL_MEMORY_TYPE_NORMAL = 0xD0;

// The smallest alignment of a block, of a CDRAM one and of a physically contiguous one. The size of
// an app's block is a multiple of it too.
constexpr SceSize MEMBLOCK_MIN_ALIGNMENT = KiB(4);
constexpr SceSize MEMBLOCK_CDRAM_MIN_ALIGNMENT = KiB(256);
constexpr SceSize MEMBLOCK_PHYCONT_MIN_ALIGNMENT = MiB(1);

// Where an app's blocks go, by user region
constexpr Address USER_CDRAM_BASE = 0x60000000;
constexpr Address USER_CDIALOG_UNCACHED_BASE = 0x68000000;
constexpr Address USER_UNCACHED_BASE = 0x70000000;
constexpr Address USER_MAIN_BASE = 0x80000000;
constexpr Address USER_CDIALOG_BASE = 0xD0000000;

struct SysmemState;

// Adds a block of the type at base to the blocks and counts it in its partition's free size, with the
// state's mutex held
SceUID add_block(SysmemState &state, const char *name, SceKernelMemBlockType type, Address base, SceSize size);

DECL_EXPORT(SceUID, ksceKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockOptKernel *opt);
DECL_EXPORT(int, ksceKernelGetMemBlockBase, SceUID uid, Ptr<void> *basep);
DECL_EXPORT(int, ksceKernelFreeMemBlock, SceUID uid);
