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

#include "SceSysmem.h"

#include <kernel/state.h>
#include <kernel/types.h>

#include <util/align.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceSysmem);

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

typedef std::shared_ptr<SceKernelMemBlockInfo> SceKernelMemBlockInfoPtr;
typedef std::map<SceUID, SceKernelMemBlockInfoPtr> Blocks;

struct SysmemState {
    std::mutex mutex;
    Blocks blocks;
    Blocks vm_blocks;
    SceUID next_uid = 1;

    SceUID get_next_uid() {
        return next_uid++;
    }
};

LIBRARY_INIT_IMPL(SceSysmem) {
    emuenv.kernel.obj_store.create<SysmemState>();
}
LIBRARY_INIT_REGISTER(SceSysmem)

constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT = 4;

EXPORT(SceUID, sceKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockOpt *optp) {
    TRACY_FUNC(sceKernelAllocMemBlock, name, type, size, optp);
    MemState &mem = emuenv.mem;
    assert(type != 0);

    if (!name || !size) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    int min_alignment;
    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RX:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
        // should only be 4K, but Freedom War crashes if the alignment is less than 32K...
        min_alignment = 0x8000;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
        min_alignment = 0x40000;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
        min_alignment = 0x100000;
        break;
    default:
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    // size should be a multiple of min_alignment

    SceSize alignment = min_alignment;
    if (optp && (optp->attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT)) {
        alignment = optp->alignment;
        // alignment must be a power of 2
        // it should also be at least min_alignment but it looks like it is not the case in games like uncharted
        // and the ps vita does not return an error
        if (alignment & (alignment - 1))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    Ptr<void> address = Ptr<void>(alloc(mem, size, name, alignment));

    if (!address) {
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }

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
    TRACY_FUNC(sceKernelAllocMemBlockForVM, name, size);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    MemState &mem = emuenv.mem;
    assert(name != nullptr);

    if (size < 0x1000 || (size & 0xFFF) != 0) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    const Ptr<void> address(alloc(mem, size, name));
    if (!address) {
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }

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
    TRACY_FUNC(sceKernelAllocUnmapMemBlock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCheckModelCapability) {
    TRACY_FUNC(sceKernelCheckModelCapability);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseMemBlock) {
    TRACY_FUNC(sceKernelCloseMemBlock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelCloseVMDomain) {
    TRACY_FUNC(sceKernelCloseVMDomain);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, sceKernelFindMemBlockByAddr, Address addr, uint32_t size) {
    TRACY_FUNC(sceKernelFindMemBlockByAddr, addr, size);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    for (auto it = state->blocks.begin(); it != state->blocks.end(); ++it) {
        if (it->second->mappedBase.address() <= addr && (it->second->mappedBase.address() + it->second->mappedSize > addr)) {
            return it->first;
        }
    }
    return RET_ERROR(SCE_KERNEL_ERROR_BLOCK_ERROR);
}

EXPORT(int, sceKernelFreeMemBlock, SceUID uid) {
    TRACY_FUNC(sceKernelFreeMemBlock, uid);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);
    assert(uid >= 0);

    const Blocks::const_iterator block = state->blocks.find(uid);
    // TODO, is really that ?
    if (block == state->blocks.end())
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID);

    free(emuenv.mem, block->second->mappedBase.address());
    state->blocks.erase(block);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelFreeMemBlockForVM, SceUID uid) {
    TRACY_FUNC(sceKernelFreeMemBlockForVM, uid);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    assert(uid >= 0);
    const Blocks::const_iterator block = state->vm_blocks.find(uid);
    assert(block != state->vm_blocks.end());

    free(emuenv.mem, block->second->mappedBase.address());
    state->blocks.erase(block);
    state->vm_blocks.erase(block);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetFreeMemorySize, SceKernelFreeMemorySizeInfo *info) {
    TRACY_FUNC(sceKernelGetFreeMemorySize, info);
    const auto free_memory = align(mem_available(emuenv.mem) / 3, 0x1000);
    info->size_cdram = free_memory;
    info->size_user = free_memory;
    info->size_phycont = free_memory;
    return STUBBED("Single pool");
}

EXPORT(int, sceKernelGetMemBlockBase, SceUID uid, Ptr<void> *basep) {
    TRACY_FUNC(sceKernelGetMemBlockBase, uid, basep);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);
    assert(uid >= 0);
    assert(basep != nullptr);

    const Blocks::const_iterator block = state->blocks.find(uid);
    if (block == state->blocks.end()) {
        // TODO Write address?
        return SCE_KERNEL_ERROR_INVALID_UID;
    }

    *basep = block->second->mappedBase.address();

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetMemBlockInfoByAddr, Address addr, SceKernelMemBlockInfo *info) {
    TRACY_FUNC(sceKernelGetMemBlockInfoByAddr, addr, info);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);
    assert(addr >= 0);
    assert(info != nullptr);
    for (Blocks::const_iterator it = state->blocks.begin(); it != state->blocks.end(); ++it) {
        if (it->second->mappedBase.address() <= addr && (it->second->mappedBase.address() + it->second->mappedSize > addr)) {
            memcpy(info, it->second.get(), sizeof(SceKernelMemBlockInfo));
            return SCE_KERNEL_OK;
        }
    }

    return SCE_KERNEL_ERROR_BLOCK_ERROR;
}

EXPORT(int, sceKernelGetMemBlockInfoByRange) {
    TRACY_FUNC(sceKernelGetMemBlockInfoByRange);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetModel) {
    TRACY_FUNC(sceKernelGetModel);
    return emuenv.cfg.current_config.pstv_mode ? SCE_KERNEL_MODEL_VITATV : SCE_KERNEL_MODEL_VITA;
}

EXPORT(int, sceKernelGetModelForCDialog) {
    TRACY_FUNC(sceKernelGetModelForCDialog);
    return emuenv.cfg.current_config.pstv_mode ? SCE_KERNEL_MODEL_VITATV : SCE_KERNEL_MODEL_VITA;
}

EXPORT(int, sceKernelGetSubbudgetInfo) {
    TRACY_FUNC(sceKernelGetSubbudgetInfo);
    return UNIMPLEMENTED();
}

EXPORT(bool, sceKernelIsPSVitaTV) {
    TRACY_FUNC(sceKernelIsPSVitaTV);
    return emuenv.cfg.current_config.pstv_mode;
}

EXPORT(int, sceKernelOpenMemBlock) {
    TRACY_FUNC(sceKernelOpenMemBlock);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelOpenVMDomain) {
    TRACY_FUNC(sceKernelOpenVMDomain);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelSyncVMDomain, SceUID block_uid, Address base, uint32_t size) {
    TRACY_FUNC(sceKernelSyncVMDomain, block_uid, base, size);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    const auto it = state->vm_blocks.find(block_uid);
    if (it == state->vm_blocks.end()) {
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID);
    }

    const auto block = it->second;
    const uint32_t block_base_end = block->mappedBase.address() + block->mappedSize;
    const uint32_t base_end = base + size;
    if (block->mappedBase.address() > base_end || base > block_base_end) {
        return RET_ERROR(SCE_KERNEL_ERROR_BLOCK_ERROR);
    }
    invalidate_jit_cache(*emuenv.kernel.get_thread(thread_id)->cpu, base, size);

    return 0;
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
