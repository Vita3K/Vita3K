// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "SceSysmemForDriver.h"

struct SceKernelAllocMemBlockOpt {
    SceSize size;
    SceUInt32 attr;
    SceSize alignment;
    SceUInt32 uidBaseBlock;
    Ptr<const char> strBaseBlockName;
    int flags;
    int reserved[10];
};
static_assert(sizeof(SceKernelAllocMemBlockOpt) == 0x40);

inline SceInt32 type_error(SceKernelMemBlockType type) {
    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_R:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_DEVICE_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_GAME_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_NC_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RO_UNCACHE:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW_UNCACHE:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RO:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_L1WBWA_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_R:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
        return 0;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_NC_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RX_UNCACHE:
        return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_TYPE;
    default:
        return SCE_KERNEL_ERROR_ILLEGAL_BLOCK_TYPE;
    }
}

inline SceInt32 size_error(SceKernelMemBlockType type, SceSize size) {
    if (size == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    switch (type & MEMBLOCK_TYPE_PHYMEMPART_MASK) {
    case MEMBLOCK_TYPE_PHYMEMPART_CDRAM:
        return size % MEMBLOCK_CDRAM_MIN_ALIGNMENT ? SCE_KERNEL_ERROR_NO_FREE_PHYSICAL_PAGE_CDRAM : 0;
    case MEMBLOCK_TYPE_PHYMEMPART_PHYCONT:
        return size % MEMBLOCK_PHYCONT_MIN_ALIGNMENT ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
    default:
        return size % MEMBLOCK_MIN_ALIGNMENT ? SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_SIZE : 0;
    }
}

inline SceInt32 alignment_error(SceKernelMemBlockType type, SceSize alignment) {
    if (alignment == 0)
        return 0;
    const SceUInt32 phymempart = type & MEMBLOCK_TYPE_PHYMEMPART_MASK;
    if ((alignment & (alignment - 1)) != 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    if (phymempart == MEMBLOCK_TYPE_PHYMEMPART_PHYCONT)
        return alignment >= MEMBLOCK_MIN_ALIGNMENT ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
    if (phymempart == MEMBLOCK_TYPE_PHYMEMPART_CDRAM) {
        if (alignment >= MEMBLOCK_MIN_ALIGNMENT && alignment < MEMBLOCK_CDRAM_MIN_ALIGNMENT)
            return SCE_KERNEL_ERROR_NO_FREE_PHYSICAL_PAGE_CDRAM;
        return alignment > MiB(64) ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
    }
    return alignment > MiB(16) ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
}
