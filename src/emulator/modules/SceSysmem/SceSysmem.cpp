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

#include "SceSysmem.h"

#include <psp2/kernel/error.h>
#include <psp2/kernel/sysmem.h>

namespace emu {
    struct SceKernelAllocMemBlockOpt;
}

using namespace emu;

EXPORT(SceUID, sceKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, int size, emu::SceKernelAllocMemBlockOpt *optp) {
    MemState &mem = host.mem;
    assert(name != nullptr);
    assert(type != 0);
    assert(size != 0);
    assert(optp == nullptr);

    const Ptr<void> address(alloc(mem, size, name));
    if (!address) {
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }

    KernelState *const state = &host.kernel;
    const SceUID uid = state->next_uid++;
    state->blocks.insert(Blocks::value_type(uid, address));

    return uid;
}

EXPORT(int, sceKernelAllocMemBlockForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelAllocUnmapMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCheckModelCapability) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseVMDomain) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelFindMemBlockByAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelFreeMemBlock, SceUID uid) {
    assert(uid >= 0);

    KernelState *const state = &host.kernel;
    const Blocks::const_iterator block = state->blocks.find(uid);
    assert(block != state->blocks.end());

    free(host.mem, block->second.address());
    state->blocks.erase(block);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelFreeMemBlockForVM) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetFreeMemorySize, SceKernelFreeMemorySizeInfo *info) {
    Address free_memory = mem_available(host.mem);
    info->size_cdram = free_memory;
    info->size_user = free_memory;
    info->size_phycont = free_memory;
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetMemBlockBase, SceUID uid, Ptr<void> *basep) {
    assert(uid >= 0);
    assert(basep != nullptr);

    const KernelState *const state = &host.kernel;
    const Blocks::const_iterator block = state->blocks.find(uid);
    if (block == state->blocks.end()) {
        // TODO Write address?
        return SCE_KERNEL_ERROR_INVALID_UID;
    }

    *basep = block->second;

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetMemBlockInfoByAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetMemBlockInfoByRange) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetModel) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetModelForCDialog) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetSubbudgetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelIsPSVitaTV) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenVMDomain) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSyncVMDomain) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceKernelAllocMemBlock)
BRIDGE_IMPL(sceKernelAllocMemBlockForVM)
BRIDGE_IMPL(sceKernelAllocUnmapMemBlock)
BRIDGE_IMPL(sceKernelCheckModelCapability)
BRIDGE_IMPL(sceKernelCloseMemBlock)
BRIDGE_IMPL(sceKernelCloseVMDomain)
BRIDGE_IMPL(sceKernelFindMemBlockByAddr)
BRIDGE_IMPL(sceKernelFreeMemBlock)
BRIDGE_IMPL(sceKernelFreeMemBlockForVM)
BRIDGE_IMPL(sceKernelGetFreeMemorySize)
BRIDGE_IMPL(sceKernelGetMemBlockBase)
BRIDGE_IMPL(sceKernelGetMemBlockInfoByAddr)
BRIDGE_IMPL(sceKernelGetMemBlockInfoByRange)
BRIDGE_IMPL(sceKernelGetModel)
BRIDGE_IMPL(sceKernelGetModelForCDialog)
BRIDGE_IMPL(sceKernelGetSubbudgetInfo)
BRIDGE_IMPL(sceKernelIsPSVitaTV)
BRIDGE_IMPL(sceKernelOpenMemBlock)
BRIDGE_IMPL(sceKernelOpenVMDomain)
BRIDGE_IMPL(sceKernelSyncVMDomain)
