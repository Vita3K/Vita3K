<<<<<<< Updated upstream
// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
=======
// RPCSV emulator project
// Copyright (C) 2025 RPCSV team
>>>>>>> Stashed changes
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

EXPORT(SceUID, ksceKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockKernelOpt *opt) {
    MemState &mem = emuenv.mem;

    if (!name || !size) {
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
        min_alignment = 0x1000;
        break;
    }

    SceSize alignment = min_alignment;
    Address base_address = 0;

    if (opt) {
        if (opt->attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_VBASE) {
            base_address = opt->field_C;
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
        // Pick start address based on type
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
            start_address = 0x80000000U;
            break;
        }
        address = Ptr<void>(alloc_aligned(mem, size, name, alignment, start_address));
        if (!address) {
            return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);
        }
    }

    const SceUID uid = state->get_next_uid();
    const auto block = std::make_shared<KernelMemBlock>();
    block->type = type;
    block->mappedBase = address;
    block->mappedSize = size;
    block->size = sizeof(SceKernelMemBlockInfo);
    std::strncpy(block->name, name, KERNELOBJECT_MAX_NAME_LENGTH);
    state->blocks.emplace(uid, block);

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
    default:
        state->allocated_user += size;
        break;
    }

    return uid;
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
