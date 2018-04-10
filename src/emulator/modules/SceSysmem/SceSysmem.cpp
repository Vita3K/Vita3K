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
    v3k_assert(name != nullptr);
    v3k_assert(type != 0);
    v3k_assert(size != 0);
    v3k_assert(optp == nullptr);

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
    return unimplemented("sceKernelAllocMemBlockForVM");
}

EXPORT(int, sceKernelAllocUnmapMemBlock) {
    return unimplemented("sceKernelAllocUnmapMemBlock");
}

EXPORT(int, sceKernelCheckModelCapability) {
    return unimplemented("sceKernelCheckModelCapability");
}

EXPORT(int, sceKernelCloseMemBlock) {
    return unimplemented("sceKernelCloseMemBlock");
}

EXPORT(int, sceKernelCloseVMDomain) {
    return unimplemented("sceKernelCloseVMDomain");
}

EXPORT(int, sceKernelFindMemBlockByAddr) {
    return unimplemented("sceKernelFindMemBlockByAddr");
}

EXPORT(int, sceKernelFreeMemBlock, SceUID uid) {
    v3k_assert(uid >= 0);

    KernelState *const state = &host.kernel;
    const Blocks::const_iterator block = state->blocks.find(uid);
    v3k_assert(block != state->blocks.end());

    free(host.mem, block->second.address());
    state->blocks.erase(block);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelFreeMemBlockForVM) {
    return unimplemented("sceKernelFreeMemBlockForVM");
}

EXPORT(int, sceKernelGetFreeMemorySize) {
    return unimplemented("sceKernelGetFreeMemorySize");
}

EXPORT(int, sceKernelGetMemBlockBase, SceUID uid, Ptr<void> *basep) {
    v3k_assert(uid >= 0);
    v3k_assert(basep != nullptr);

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
    return unimplemented("sceKernelGetMemBlockInfoByAddr");
}

EXPORT(int, sceKernelGetMemBlockInfoByRange) {
    return unimplemented("sceKernelGetMemBlockInfoByRange");
}

EXPORT(int, sceKernelGetModel) {
    return unimplemented("sceKernelGetModel");
}

EXPORT(int, sceKernelGetModelForCDialog) {
    return unimplemented("sceKernelGetModelForCDialog");
}

EXPORT(int, sceKernelGetSubbudgetInfo) {
    return unimplemented("sceKernelGetSubbudgetInfo");
}

EXPORT(int, sceKernelIsPSVitaTV) {
    return unimplemented("sceKernelIsPSVitaTV");
}

EXPORT(int, sceKernelOpenMemBlock) {
    return unimplemented("sceKernelOpenMemBlock");
}

EXPORT(int, sceKernelOpenVMDomain) {
    return unimplemented("sceKernelOpenVMDomain");
}

EXPORT(int, sceKernelSyncVMDomain) {
    return unimplemented("sceKernelSyncVMDomain");
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
