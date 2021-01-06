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

#include <kernel/state.h>
#include <kernel/types.h>

struct SceKernelAllocMemBlockOpt {
    SceSize size;
    SceUInt32 attr;
    SceSize alignment;
    SceUInt32 uidBaseBlock;
    const char *strBaseBlockName;
    int flags;
    int reserved[10];
};

struct SceKernelFreeMemorySizeInfo {
    int size; //!< sizeof(SceKernelFreeMemorySizeInfo)
    int size_user; //!< Free memory size for *_USER_RW memory
    int size_cdram; //!< Free memory size for USER_CDRAM_RW memory
    int size_phycont; //!< Free memory size for USER_MAIN_PHYCONT_*_RW memory
};

EXPORT(SceUID, sceKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockOpt *optp) {
    MemState &mem = host.mem;
    assert(name != nullptr);
    assert(type != 0);

    if (size < 0x1000 || (size & ~0xFFF) != 0) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    Ptr<void> address;
    if (optp == nullptr) {
        address = alloc(mem, size, name);
    } else {
        address = alloc(mem, size, name, optp->alignment);
    }
    if (!address) {
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }

    auto guard = std::lock_guard<std::mutex>(host.kernel.mutex);
    KernelState *const state = &host.kernel;
    const SceUID uid = state->get_next_uid();

    const SceKernelMemBlockInfoPtr sceKernelMemBlockInfo = std::make_shared<SceKernelMemBlockInfo>();
    sceKernelMemBlockInfo->type = type;
    sceKernelMemBlockInfo->mappedBase = address;
    sceKernelMemBlockInfo->mappedSize = size;
    sceKernelMemBlockInfo->size = sizeof(SceKernelMemBlockInfo);
    state->blocks.insert(Blocks::value_type(uid, sceKernelMemBlockInfo));

    return uid;
}

EXPORT(int, sceKernelAllocMemBlockForVM, const char *name, SceSize size) {
    MemState &mem = host.mem;
    assert(name != nullptr);

    if (size < 0x1000 || (size & ~0xFFF) != 0) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    const Ptr<void> address(alloc(mem, size, name));
    if (!address) {
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }

    auto guard = std::lock_guard<std::mutex>(host.kernel.mutex);
    KernelState *const state = &host.kernel;
    const SceUID uid = state->get_next_uid();

    const SceKernelMemBlockInfoPtr sceKernelMemBlockInfo = std::make_shared<SceKernelMemBlockInfo>();
    sceKernelMemBlockInfo->mappedBase = address;
    sceKernelMemBlockInfo->mappedSize = size;
    sceKernelMemBlockInfo->size = sizeof(SceKernelMemBlockInfo);
    state->blocks.insert(Blocks::value_type(uid, sceKernelMemBlockInfo));
    state->vm_blocks.insert(Blocks::value_type(uid, sceKernelMemBlockInfo));

    return uid;
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

EXPORT(SceUID, sceKernelFindMemBlockByAddr, Address addr, uint32_t size) {
    auto guard = std::lock_guard<std::mutex>(host.kernel.mutex);

    for (auto it = host.kernel.blocks.begin(); it != host.kernel.blocks.end(); ++it) {
        if (it->second->mappedBase.address() <= addr && (it->second->mappedBase.address() + it->second->mappedSize > addr)) {
            return it->first;
        }
    }
    return RET_ERROR(SCE_KERNEL_ERROR_BLOCK_ERROR);
}

EXPORT(int, sceKernelFreeMemBlock, SceUID uid) {
    assert(uid >= 0);

    auto guard = std::lock_guard<std::mutex>(host.kernel.mutex);
    KernelState *const state = &host.kernel;
    const Blocks::const_iterator block = state->blocks.find(uid);
    // TODO, is really that ?
    if (block == state->blocks.end())
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID);

    free(host.mem, block->second->mappedBase.address());
    state->blocks.erase(block);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelFreeMemBlockForVM, SceUID uid) {
    assert(uid >= 0);

    auto guard = std::lock_guard<std::mutex>(host.kernel.mutex);
    KernelState *const state = &host.kernel;
    const Blocks::const_iterator block = state->vm_blocks.find(uid);
    assert(block != state->vm_blocks.end());

    free(host.mem, block->second->mappedBase.address());
    state->blocks.erase(block);
    state->vm_blocks.erase(block);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetFreeMemorySize, SceKernelFreeMemorySizeInfo *info) {
    Address free_memory = mem_available(host.mem);
    info->size_cdram = free_memory / 3;
    info->size_user = free_memory / 3;
    info->size_phycont = free_memory / 3;
    return STUBBED("Single pool");
}

EXPORT(int, sceKernelGetMemBlockBase, SceUID uid, Ptr<void> *basep) {
    assert(uid >= 0);
    assert(basep != nullptr);

    auto guard = std::lock_guard<std::mutex>(host.kernel.mutex);
    const KernelState *const state = &host.kernel;
    const Blocks::const_iterator block = state->blocks.find(uid);
    if (block == state->blocks.end()) {
        // TODO Write address?
        return SCE_KERNEL_ERROR_INVALID_UID;
    }

    *basep = block->second->mappedBase.address();

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetMemBlockInfoByAddr, Address addr, SceKernelMemBlockInfo *info) {
    assert(addr >= 0);
    assert(info != nullptr);

    auto guard = std::lock_guard<std::mutex>(host.kernel.mutex);
    const KernelState *const state = &host.kernel;
    for (Blocks::const_iterator it = state->blocks.begin(); it != state->blocks.end(); ++it) {
        if (it->second->mappedBase.address() <= addr && (it->second->mappedBase.address() + it->second->mappedSize > addr)) {
            memcpy(info, it->second.get(), sizeof(SceKernelMemBlockInfo));
            return SCE_KERNEL_OK;
        }
    }

    return SCE_KERNEL_ERROR_BLOCK_ERROR;
}

EXPORT(int, sceKernelGetMemBlockInfoByRange) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetModel) {
    return host.cfg.pstv_mode ? SCE_KERNEL_MODEL_VITATV : SCE_KERNEL_MODEL_VITA;
}

EXPORT(int, sceKernelGetModelForCDialog) {
    return host.cfg.pstv_mode ? SCE_KERNEL_MODEL_VITATV : SCE_KERNEL_MODEL_VITA;
}

EXPORT(int, sceKernelGetSubbudgetInfo) {
    return UNIMPLEMENTED();
}

EXPORT(bool, sceKernelIsPSVitaTV) {
    return host.cfg.pstv_mode;
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
