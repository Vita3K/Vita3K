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

#include "SceSysmem.h"
#include "SceSysmemForDriver.h"

#include <emuenv/state.h>
#include <kernel/state.h>
#include <kernel/types.h>
#include <mem/functions.h>
#include <modules/sysmem_state.h>

#include <packages/sfo.h>

#include <util/align.h>
#include <util/fs.h>
#include <util/string_utils.h>

#include <array>
#include <utility>

#include <util/tracy.h>

#include <algorithm>
TRACY_MODULE_NAME(SceSysmem);

std::string to_debug_str(const MemState &mem, SceKernelMemBlockType type) {
    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RX: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_RX";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_RW";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW";
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW: return "SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW";
    default: break;
    }
    return std::to_string(type);
}

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

struct SceKernelFreeMemorySizeInfo {
    int size; //!< sizeof(SceKernelFreeMemorySizeInfo)
    int size_user; //!< Free memory size for *_USER_RW memory
    int size_cdram; //!< Free memory size for USER_CDRAM_RW memory
    int size_phycont; //!< Free memory size for USER_MAIN_PHYCONT_*_RW memory
};

LIBRARY_INIT(SceSysmem) {
    emuenv.kernel.obj_store.create<SysmemState>();
}

// The error for a block type an app can't allocate, or 0
static SceInt32 type_error(SceKernelMemBlockType type) {
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
    // Types a Vita knows but keeps from apps
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_NC_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RX_UNCACHE:
        return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_TYPE;
    default:
        return SCE_KERNEL_ERROR_ILLEGAL_BLOCK_TYPE;
    }
}

// The error for a size a block of a user type can't have, or 0
static SceInt32 size_error(SceKernelMemBlockType type, SceSize size) {
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

// The error for an alignment a block of a user type can't ask for, or 0
static SceInt32 alignment_error(SceKernelMemBlockType type, SceSize alignment) {
    // 0 asks for no alignment, which every type takes
    if (alignment == 0)
        return 0;
    const SceUInt32 phymempart = type & MEMBLOCK_TYPE_PHYMEMPART_MASK;
    if (phymempart == MEMBLOCK_TYPE_PHYMEMPART_PHYCONT || (alignment & (alignment - 1)) != 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    if (phymempart == MEMBLOCK_TYPE_PHYMEMPART_CDRAM) {
        if (alignment < MEMBLOCK_CDRAM_MIN_ALIGNMENT)
            return SCE_KERNEL_ERROR_NO_FREE_PHYSICAL_PAGE_CDRAM;
        return alignment > MiB(64) ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
    }
    return alignment > MiB(16) ? SCE_KERNEL_ERROR_INVALID_ARGUMENT : 0;
}

EXPORT(SceUID, sceKernelAllocMemBlock, const char *pName, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockOpt *optp) {
    TRACY_FUNC(sceKernelAllocMemBlock, pName, type, size, optp);
    if (!pName)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    if (optp) {
        constexpr SceSize OPT_SIZES[] = { 0x14, 0x38, 0x3C, 0x40 };
        // An app can't choose an address, a physical address or a base block
        constexpr SceUInt32 ALLOWED_ATTRS = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_ENABLE_OPEN
            | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
        if (!std::ranges::contains(OPT_SIZES, optp->size))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
        // The last word of the largest option must be 0
        if (optp->size == sizeof(SceKernelAllocMemBlockOpt) && optp->reserved[9] != 0)
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
        if (optp->attr & ~ALLOWED_ATTRS)
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }
    if (const SceInt32 error = type_error(type))
        return RET_ERROR(error);
    if (const SceInt32 error = size_error(type, size))
        return RET_ERROR(error);

    // Build kernel opts from user opts
    SceKernelAllocMemBlockOptKernel k_opt = {};
    k_opt.size = sizeof(k_opt);
    if (optp && (optp->attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT)) {
        if (const SceInt32 error = alignment_error(type, optp->alignment))
            return RET_ERROR(error);
        k_opt.attr |= SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
        k_opt.alignment = optp->alignment;
    }

    return CALL_EXPORT(ksceKernelAllocMemBlock, pName, type, size, optp ? &k_opt : nullptr);
}

EXPORT(SceUID, sceKernelAllocMemBlockForVM, const char *pName, SceSize size) {
    TRACY_FUNC(sceKernelAllocMemBlockForVM, pName, size);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    std::unique_lock<std::mutex> lock(state->mutex);
    if (!pName)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    // A size of 0 gets past this, and the allocation fails on it
    if (size % MiB(1))
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_SIZE);
    if (size > MiB(16) - state->vm_space_used)
        return RET_ERROR(SCE_KERNEL_ERROR_MEMBLOCK_OVERFLOW);
    state->vm_space_used += size;
    lock.unlock();

    const SceUID uid = CALL_EXPORT(ksceKernelAllocMemBlock, pName, SCE_KERNEL_MEMBLOCK_TYPE_USER_RX, size, nullptr);
    lock.lock();
    // The block is missing if the allocation failed, or if another thread freed it before the lock
    const Blocks::const_iterator block = state->blocks.find(uid);
    if (block == state->blocks.end()) {
        state->vm_space_used -= size;
        return uid < 0 ? uid : RET_ERROR(SCE_KERNEL_ERROR_INVALID_UID);
    }
    state->vm_blocks.emplace(uid, SceKernelVARange{ block->second->mappedBase.address(), size });
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
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);
    if (!state->vm_domain_open)
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    state->vm_domain_open = false;
    return SCE_KERNEL_OK;
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
    return CALL_EXPORT(ksceKernelFreeMemBlock, uid);
}

EXPORT(int, sceKernelFreeMemBlockForVM, SceUID uid) {
    TRACY_FUNC(sceKernelFreeMemBlockForVM, uid);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    {
        const auto guard = std::lock_guard<std::mutex>(state->mutex);
        const auto block = state->vm_blocks.find(uid);
        if (block == state->vm_blocks.end())
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
        state->vm_space_used -= block->second.size;
        state->vm_blocks.erase(block);
    }
    return CALL_EXPORT(ksceKernelFreeMemBlock, uid);
}

uint32_t get_max_user_memory(EmuEnvState &emuenv) {
    // Default memory configuration
    uint32_t max_user = MiB(256);

    // if DevKit then max_user = MB(512); else check sfo file for memory expansion mode
    // Fetch the "ATTRIBUTE2" key from the SFO file to check for memory expansion mode
    std::string attribute2;
    if (sfo::get_data_by_key(attribute2, emuenv.sfo_handle, "ATTRIBUTE2")) {
        // Convert the string to an unsigned 32-bit integer
        const uint32_t attr_val = string_utils::stoi_def(attribute2, 0, "memory expansion mode");

        switch (attr_val & 0x0C) {
        case 0x4: // Check for the +29MiB mode
            max_user += MiB(29);
            break;
        case 0x8: // Check for the +77MiB mode
            max_user += MiB(77);
            break;
        case 0xC: // Check for the +109MiB mode
            max_user += MiB(109);
            break;
        default: break;
        }
    } else
        LOG_WARN_ONCE("ATTRIBUTE2 key not found in SFO data.");

    return max_user;
}

EXPORT(int, sceKernelGetFreeMemorySize, SceKernelFreeMemorySizeInfo *info) {
    TRACY_FUNC(sceKernelGetFreeMemorySize, info);

    const uint32_t max_user = get_max_user_memory(emuenv);

    // Define other memory limits
    constexpr uint32_t max_cdram = MiB(112); // Max cdram memory (112 MiB)
    constexpr uint32_t max_phycont = MiB(26); // Max physically contiguous memory (26 MiB)
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    // Set the free memory size info
    info->size_cdram = std::max<int>(max_cdram - state->allocated_cdram, 0);
    info->size_user = std::max<int>(max_user - state->allocated_user, 0);
    info->size_phycont = std::max<int>(max_phycont - state->allocated_phycont, 0);

    return 0;
}

EXPORT(int, sceKernelGetMemBlockBase, SceUID uid, Ptr<void> *basep) {
    TRACY_FUNC(sceKernelGetMemBlockBase, uid, basep);
    return CALL_EXPORT(ksceKernelGetMemBlockBase, uid, basep);
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

void create_system_font_blocks(EmuEnvState &emuenv) {
    constexpr std::array<std::pair<const char *, const char *>, 2> fonts = { {
        { "SceSysDefaultLtnFont", "ltn0.pvf" },
        { "SceSysDefaultJpnFont", "jpn0.pvf" },
    } };

    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const std::lock_guard<std::mutex> guard(state->mutex);

    for (const auto &[name, file] : fonts) {
        std::vector<uint8_t> data;
        if (!fs_utils::read_data(emuenv.vita_fs_path / "sa0/data/font/pvf" / file, data) || data.empty()) {
            LOG_WARN("Missing {} for {}, install the firmware font package", file, name);
            continue;
        }

        const uint32_t size = align(static_cast<uint32_t>(data.size()), 0x1000);
        // Below main memory and far above the uncached range, so app allocations keep their addresses.
        const Ptr<uint8_t> address(alloc_aligned(emuenv.mem, size, name, 0x1000, 0x7F000000));
        if (!address)
            continue;
        memcpy(address.get(emuenv.mem), data.data(), data.size());

        // Not add_block: the shell owns these blocks, so they must not count against the app's memory.
        const auto block = std::make_shared<KernelMemBlock>();
        block->type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
        block->mappedBase = address;
        block->mappedSize = size;
        block->size = sizeof(SceKernelMemBlockInfo);
        block->memoryType = SCE_KERNEL_MEMORY_TYPE_NORMAL;
        block->access = SCE_KERNEL_MEMORY_ACCESS_R | SCE_KERNEL_MEMORY_ACCESS_W;
        std::strncpy(block->name, name, KERNELOBJECT_MAX_NAME_LENGTH);
        state->blocks.emplace(state->get_next_uid(), block);
    }
}

EXPORT(SceUID, sceKernelOpenMemBlock, const char *pName, int flags) {
    TRACY_FUNC(sceKernelOpenMemBlock, pName, flags);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const std::lock_guard<std::mutex> memblock_lock(state->mutex);

    const auto it = std::ranges::find_if(state->blocks, [=](const auto &block) {
        return strncmp(block.second->name, pName, KERNELOBJECT_MAX_NAME_LENGTH) == 0;
    });

    if (it != state->blocks.end())
        return it->first;

    return RET_ERROR(SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME);
}

EXPORT(int, sceKernelOpenVMDomain) {
    TRACY_FUNC(sceKernelOpenVMDomain);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);
    // Blocks for code are always writable here, so the domain is only a state
    if (state->vm_domain_open)
        return RET_ERROR(SCE_ERROR_ERRNO_ENOSYS);
    state->vm_domain_open = true;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelSyncVMDomain, SceUID block_uid, Address base, uint32_t size) {
    TRACY_FUNC(sceKernelSyncVMDomain, block_uid, base, size);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    // The blocks for code first, then any block
    SceKernelVARange range;
    if (const auto vm_block = state->vm_blocks.find(block_uid); vm_block != state->vm_blocks.end())
        range = vm_block->second;
    else if (const auto block = state->blocks.find(block_uid); block != state->blocks.end())
        range = { block->second->mappedBase.address(), block->second->mappedSize };
    else
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID);
    if (base < range.addr || uint64_t(base) + size > uint64_t(range.addr) + range.size)
        return RET_ERROR(SCE_KERNEL_ERROR_MEMBLOCK_RANGE_ERROR);
    invalidate_jit_cache(*emuenv.kernel.get_thread(thread_id)->cpu, base, size);

    return 0;
}
