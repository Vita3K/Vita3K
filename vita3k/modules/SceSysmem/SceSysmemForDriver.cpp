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

#include "SceSysmemForDriver.h"
#include <modules/sysmem_state.h>

#include <kernel/state.h>
#include <mem/functions.h>
#include <util/align.h>

#include <util/log.h>

#include <cstring>

EXPORT(int, ksceGUIDClose) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceGUIDReferObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceGUIDReferObjectWithClass) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceGUIDReferObjectWithClassLevel) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceGUIDReleaseObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelAddressSpaceVAtoPABySW) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelAllocHeapMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelAllocHeapMemoryFromGlobalHeap) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelAllocHeapMemoryFromGlobalHeapWithOpt) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelAllocHeapMemoryWithOpt1) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelAllocHeapMemoryWithOption) {
    return UNIMPLEMENTED();
}

// Where blocks of a user type go: CDRAM, the common dialog's memory, uncached main memory or main
// memory. Kernel types go to main memory.
static Address region_start(SceKernelMemBlockType type) {
    if (type & MEMBLOCK_TYPE_KERNEL_MASK)
        return USER_MAIN_BASE;
    const SceUInt32 partition_index = type & MEMBLOCK_TYPE_PARTITION_INDEX_MASK;
    const bool uncached = (type & MEMBLOCK_TYPE_MEMORY_ATTR_MASK) == MEMBLOCK_TYPE_MEMORY_ATTR_NORMAL_NC;
    if (partition_index == MEMBLOCK_TYPE_PARTITION_INDEX_USER_CDRAM)
        return USER_CDRAM_BASE;
    if (partition_index == MEMBLOCK_TYPE_PARTITION_INDEX_USER_CDIALOG)
        return uncached ? USER_CDIALOG_UNCACHED_BASE : USER_CDIALOG_BASE;
    return uncached ? USER_UNCACHED_BASE : USER_MAIN_BASE;
}

// The smallest alignment of a block of the type, which for a user type depends on its ScePhyMemPart
static SceSize min_alignment(SceKernelMemBlockType type) {
    if (type & MEMBLOCK_TYPE_KERNEL_MASK)
        return MEMBLOCK_MIN_ALIGNMENT;
    switch (type & MEMBLOCK_TYPE_PHYMEMPART_MASK) {
    case MEMBLOCK_TYPE_PHYMEMPART_CDRAM:
        return MEMBLOCK_CDRAM_MIN_ALIGNMENT;
    case MEMBLOCK_TYPE_PHYMEMPART_PHYCONT:
        return MEMBLOCK_PHYCONT_MIN_ALIGNMENT;
    default:
        return MEMBLOCK_MIN_ALIGNMENT;
    }
}

// Adds size, negative for a freed block, to the allocated count of the partition a block of the type
// takes from. Common dialog blocks take from none.
static void count_block(SysmemState &state, SceKernelMemBlockType type, SceSSize size) {
    if ((type & MEMBLOCK_TYPE_PARTITION_INDEX_MASK) == MEMBLOCK_TYPE_PARTITION_INDEX_USER_CDIALOG)
        return;
    switch (type & MEMBLOCK_TYPE_PHYMEMPART_MASK) {
    case MEMBLOCK_TYPE_PHYMEMPART_CDRAM:
        state.allocated_cdram += size;
        break;
    case MEMBLOCK_TYPE_PHYMEMPART_PHYCONT:
        state.allocated_phycont += size;
        break;
    case MEMBLOCK_TYPE_PHYMEMPART_CDIALOG:
        break;
    default:
        state.allocated_user += size;
        break;
    }
}

SceUID add_block(SysmemState &state, const char *name, SceKernelMemBlockType type, Address base, SceSize size) {
    const SceUID uid = state.get_next_uid();
    const auto block = std::make_shared<KernelMemBlock>();
    block->type = type;
    block->mappedBase = Ptr<void>(base);
    block->mappedSize = size;
    block->size = sizeof(SceKernelMemBlockInfo);
    // What the info calls report: the normal memory type bits of the memory attributes, and an app's
    // read and write access
    block->memoryType = ((type & MEMBLOCK_TYPE_MEMORY_ATTR_MASK) >> MEMBLOCK_TYPE_MEMORY_ATTR_SHIFT) & SCE_KERNEL_MEMORY_TYPE_NORMAL;
    block->access = ((type & MEMBLOCK_TYPE_USER_ACCESS_MASK) >> MEMBLOCK_TYPE_USER_ACCESS_SHIFT) & (SCE_KERNEL_MEMORY_ACCESS_R | SCE_KERNEL_MEMORY_ACCESS_W);
    std::strncpy(block->name, name, KERNELOBJECT_MAX_NAME_LENGTH);
    state.blocks.emplace(uid, block);
    count_block(state, type, size);
    return uid;
}

EXPORT(SceUID, ksceKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockOptKernel *opt) {
    MemState &mem = emuenv.mem;

    if (!name || !size) {
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    SceSize alignment = min_alignment(type);
    Address base_address = 0;

    if (opt) {
        constexpr SceUInt32 IMPLEMENTED_ATTRS = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_VBASE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
        if (const SceUInt32 ignored = opt->attr & ~IMPLEMENTED_ATTRS)
            LOG_WARN("ksceKernelAllocMemBlock: ignoring attributes {} of block {}", log_hex(ignored), name);
        if (opt->attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_VBASE) {
            base_address = opt->vbase.address();
        }
        if (opt->attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT) {
            if (opt->alignment & (opt->alignment - 1))
                return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
            alignment = std::max(alignment, opt->alignment);
        }
    }

    alignment = std::max(alignment, size & -size);

    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    Ptr<void> address;
    if (base_address) {
        Address addr = alloc_at(mem, base_address, size, name);
        if (!addr) {
            return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
        }
        address = Ptr<void>(addr);
    } else {
        address = Ptr<void>(alloc_aligned(mem, size, name, alignment, region_start(type)));
        if (!address) {
            return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
        }
    }

    return add_block(*state, name, type, address.address(), size);
}

EXPORT(int, ksceKernelAllocMemBlockWithInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateClass) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateHeap) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateUidObj2) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateUidObjForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateUserUidForClass) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateUserUidForName) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelCreateUserUidForNameWithClass) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelDeleteHeap) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFindMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFindMemBlockByAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFindMemBlockByAddrForDefaultSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFindMemBlockByAddrForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFindMemBlockForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFirstDifferentBlock32User) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFirstDifferentBlock64User) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFirstDifferentBlock64UserForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFirstDifferentIntUserForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFreeHeapMemory) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFreeHeapMemoryFromGlobalHeap) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelFreeMemBlock, SceUID uid) {
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    const Blocks::const_iterator block = state->blocks.find(uid);
    if (block == state->blocks.end())
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_UID);

    free(emuenv.mem, block->second->mappedBase.address());
    count_block(*state, block->second->type, -static_cast<SceSSize>(block->second->mappedSize));
    state->blocks.erase(block);
    return SCE_KERNEL_OK;
}

EXPORT(int, ksceKernelGUIDGetObject) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetClassForPidForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetClassForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetMemBlockBase, SceUID uid, Ptr<void> *basep) {
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    const Blocks::const_iterator block = state->blocks.find(uid);
    if (block == state->blocks.end())
        return SCE_KERNEL_ERROR_INVALID_UID;

    *basep = block->second->mappedBase.address();
    return SCE_KERNEL_OK;
}

EXPORT(int, ksceKernelGetMemBlockMappedBase) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetMemBlockPARange) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetMemBlockPaddrListForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetMemBlockVBase) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetNameForPidByUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetNameForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetNameForUid2) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetObjectForPidForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetObjectForUidForAttr) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetObjectForUidForClassTree) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetPaddrListForLargePage) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetPaddrListForSmallPage) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetPaddrPair) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetPaddrPairForLargePage) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetPaddrPairForSmallPage) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetPhysicalMemoryType) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetPidContext) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetUidClass) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelIsPaddrWithinSameSectionForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelKernelUidForUserUidForClass) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMapBlockUserVisible) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMapBlockUserVisibleWithFlag) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMapUserBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemBlockDecRefCounterAndReleaseUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemBlockGetInfoEx) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemBlockGetInfoExForVisibilityLevel) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemBlockGetSomeSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemBlockIncRefCounterAndReleaseUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemBlockRelease) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemBlockType2Memtype) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemBlockTypeGetPrivileges) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemRangeRelease) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemRangeReleaseForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemRangeReleaseWithPerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemRangeRetain) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemRangeRetainForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemRangeRetainWithPerm) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemcpyKernelToUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemcpyKernelToUserForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemcpyKernelToUserForPidUnchecked) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemcpyUserToKernel, Ptr<void> dst, Ptr<const void> src, SceSize len) {
    memcpy(dst.get(emuenv.mem), src.get(emuenv.mem), len);
    return 0;
}

EXPORT(int, ksceKernelMemcpyUserToKernelForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemcpyUserToUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelMemcpyUserToUserForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelOpenUidForName) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelProcModeVAtoPA) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelProcUserMap) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelRemapBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelRoMemcpyKernelToUserForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSetNameForPidForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSetObjectForUid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelStrncpyKernelToUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelStrncpyUserForPid) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, ksceKernelStrncpyUserToKernel, Ptr<char> dst, Ptr<const char> src, SceSize len) {
    strncpy(dst.get(emuenv.mem), src.get(emuenv.mem), len);
    return static_cast<SceInt32>(strnlen(dst.get(emuenv.mem), len));
}

EXPORT(int, ksceKernelStrnlenUser) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelStrnlenUserForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSwitchPidContext) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelSwitchVmaForPid) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelUnmapMemBlock) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelUserMap) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelVARangeToPAVector) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelVAtoPA) {
    return UNIMPLEMENTED();
}

EXPORT(int, kscePUIDClose) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, kscePUIDOpenByGUID, SceUID pid, SceUID guid) {
    // In the emulator there is no user/kernel UID separation.
    // Return the same UID (GUID == PUID).
    return guid;
}

EXPORT(int, kscePUIDtoGUID) {
    return UNIMPLEMENTED();
}
