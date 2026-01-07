// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <packages/sfo.h>

#include <util/align.h>
#include <util/string_utils.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceSysmem);

template <>
std::string to_debug_str<SceKernelMemBlockType>(const MemState &mem, SceKernelMemBlockType type) {
    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RX: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_RX";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_RW";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW";
    }
    return std::to_string(type);
}

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

struct KernelMemBlock : SceKernelMemBlockInfo {
    char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
};

typedef std::shared_ptr<KernelMemBlock> KernelMemBlockPtr;
typedef std::map<SceUID, KernelMemBlockPtr> Blocks;

struct SysmemState {
    std::mutex mutex;
    Blocks blocks;
    Blocks vm_blocks;
    SceUID next_uid = 1;

    uint32_t allocated_user = 0;
    uint32_t allocated_cdram = 0;
    uint32_t allocated_phycont = 0;

    SceUID get_next_uid() {
        return next_uid++;
    }
};

LIBRARY_INIT(SceSysmem) {
    emuenv.kernel.obj_store.create<SysmemState>();
}

constexpr SceUInt32 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT = 4;

EXPORT(SceUID, sceKernelAllocMemBlock, const char *pName, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockOpt *optp) {
    TRACY_FUNC(sceKernelAllocMemBlock, pName, type, size, optp);
    MemState &mem = emuenv.mem;
    assert(type != 0);

    if (!pName || !size) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    int min_alignment;
    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RX:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
        min_alignment = 0x1000;
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
        // alignment must be a power of 2
        if (optp->alignment & (optp->alignment - 1))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

        alignment = std::max(alignment, optp->alignment);
    }

    // x & -x returns the lsb of x
    alignment = std::max(alignment, size & -size);

    // https://wiki.henkaku.xyz/vita/SceSysmem_Types#memtype_bit_value
    Address start_address;
    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
        start_address = 0x70000000U;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
        start_address = 0x60000000U;
        break;
    default:
        // technically should be 0x81000000 but it shouldn't make a difference
        start_address = 0x80000000U;
        break;
    }

    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    Ptr<void> address = Ptr<void>(alloc_aligned(mem, size, pName, alignment, start_address));

    if (!address) {
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }

    const SceUID uid = state->get_next_uid();

    const KernelMemBlockPtr sceKernelMemBlock = std::make_shared<KernelMemBlock>();
    sceKernelMemBlock->type = type;
    sceKernelMemBlock->mappedBase = address;
    sceKernelMemBlock->mappedSize = size;
    sceKernelMemBlock->size = sizeof(SceKernelMemBlockInfo);
    std::strncpy(sceKernelMemBlock->name, pName, KERNELOBJECT_MAX_NAME_LENGTH);
    state->blocks.emplace(uid, sceKernelMemBlock);

    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RX:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
        state->allocated_user += size;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
        state->allocated_cdram += size;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
        state->allocated_phycont += size;
        break;
    }

    return uid;
}

EXPORT(int, sceKernelAllocMemBlockForVM, const char *pName, SceSize size) {
    TRACY_FUNC(sceKernelAllocMemBlockForVM, pName, size);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    MemState &mem = emuenv.mem;
    assert(pName != nullptr);

    if (size < 0x1000 || (size & 0xFFF) != 0) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    const Ptr<void> address(alloc(mem, size, pName));
    if (!address) {
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
    }

    const SceUID uid = state->get_next_uid();

    const KernelMemBlockPtr sceKernelMemBlock = std::make_shared<KernelMemBlock>();
    sceKernelMemBlock->mappedBase = address;
    sceKernelMemBlock->mappedSize = size;
    sceKernelMemBlock->size = sizeof(SceKernelMemBlockInfo);
    std::strncpy(sceKernelMemBlock->name, pName, KERNELOBJECT_MAX_NAME_LENGTH);
    state->blocks.emplace(uid, sceKernelMemBlock);
    state->vm_blocks.emplace(uid, sceKernelMemBlock);
    state->allocated_user += size;

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

    for (auto &[id, block] : state->blocks) {
        if (block->mappedBase.address() <= addr && (block->mappedBase.address() + block->mappedSize > addr)) {
            return id;
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

    switch (block->second->type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RX:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
        state->allocated_user -= block->second->size;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
        state->allocated_cdram -= block->second->size;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
        state->allocated_phycont -= block->second->size;
        break;
    default:
        state->allocated_user -= block->second->size;
        break;
    }

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
    state->allocated_user -= block->second->size;
    state->blocks.erase(block);
    state->vm_blocks.erase(block);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetFreeMemorySize, SceKernelFreeMemorySizeInfo *info) {
    TRACY_FUNC(sceKernelGetFreeMemorySize, info);

    // Default memory configuration
    uint32_t max_user = MiB(256);

    // if DevKit then max_user = MB(512); else check sfo file for memory expansion mode
    // Fetch the "ATTRIBUTE2" key from the SFO file to check for memory expansion mode
    std::string attribute2;
    if (sfo::get_data_by_key(attribute2, emuenv.sfo_handle, "ATTRIBUTE2")) {
        // Convert the string to an unsigned 32-bit integer
        const uint32_t attr_val = string_utils::stoi_def(attribute2, 0, "memory expansion mode");
        // LOG_DEBUG("val: {}.", attr_val);
        switch (attr_val & 0x0C) {
        case 0x4: // Check for the +29MiB mode
            LOG_DEBUG("Is +29MiB mode.");
            max_user += MiB(29);
            break;
        case 0x8: // Check for the +77MiB mode
            LOG_DEBUG("Is +77MiB mode.");
            max_user += MiB(77);
            break;
        case 0xC: // Check for the +109MiB mode
            LOG_DEBUG("Is +109MiB mode.");
            max_user += MiB(109);
            break;
        default: break;
        }
    } else
        LOG_WARN_ONCE("ATTRIBUTE2 key not found in SFO data.");

    // Define other memory limits
    constexpr uint32_t max_cdram = MiB(112); // Max cdram memory (112 MiB)
    constexpr uint32_t max_phycont = MiB(26); // Max physically contiguous memory (26 MiB)
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    // Set the free memory size info
    info->size_cdram = std::max<int>(max_cdram - state->allocated_cdram, 0);
    info->size_user = std::max<int>(max_user - state->allocated_user, 0);
    info->size_phycont = std::max<int>(max_phycont - state->allocated_phycont, 0);
    // LOG_DEBUG("Free memory size: user={} cdram={} phycont={}", info->size_user, info->size_cdram, info->size_phycont);
    return 0;
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
    for (const auto &[_, block_info] : state->blocks) {
        if (block_info->mappedBase.address() <= addr && (block_info->mappedBase.address() + block_info->mappedSize > addr)) {
            memcpy(info, block_info.get(), sizeof(SceKernelMemBlockInfo));
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

EXPORT(SceUID, sceKernelOpenMemBlock, const char *pName, int flags) {
    TRACY_FUNC(sceKernelOpenMemBlock, pName, flags);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const std::lock_guard<std::mutex> memblock_lock(state->mutex);

    const auto it = std::find_if(state->blocks.begin(), state->blocks.end(), [=](const auto &block) {
        return strncmp(block.second->name, pName, KERNELOBJECT_MAX_NAME_LENGTH) == 0;
    });

    if (it != state->blocks.end())
        return it->first;

    return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);
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
