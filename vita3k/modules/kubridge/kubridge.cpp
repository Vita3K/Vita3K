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

// HLE overrides for kubridge exports that cannot run as LLE.
// kubridge's mem*/exception functions depend on firmware-specific offsets
// and ARM MMU page table manipulation, which don't work in an emulator.
// These HLE implementations provide equivalent functionality using
// Vita3K's memory system.

#include <module/module.h>

#include <cpu/functions.h>
#include <kernel/state.h>
#include <mem/functions.h>
#include <modules/sysmem_state.h>
#include <util/align.h>
#include <util/log.h>
#include <util/tracy.h>

TRACY_MODULE_NAME(kubridge);

// kubridge protection flags
constexpr uint32_t KU_KERNEL_PROT_NONE = 0x00;
constexpr uint32_t KU_KERNEL_PROT_READ = 0x40;
constexpr uint32_t KU_KERNEL_PROT_WRITE = 0x20;
constexpr uint32_t KU_KERNEL_PROT_EXEC = 0x10;

// kubridge commit opt attr
constexpr uint32_t KU_KERNEL_MEM_COMMIT_ATTR_HAS_BASE = 0x1;

struct KuKernelMemCommitOpt {
    uint32_t size;
    uint32_t attr;
    SceUID baseBlock;
    uint32_t baseOffset;
};

struct KuKernelExceptionHandlerOpt {
    uint32_t size;
};

namespace {
bool normalize_range(Address addr, SceSize len, uint32_t &size) {
    const uint64_t rounded = (uint64_t(len) + 4095) & ~uint64_t(4095);
    if (addr < 0x40000000u || (addr & 4095) || !rounded || rounded > UINT32_MAX || uint64_t(addr) + rounded > (uint64_t{ 1 } << 32))
        return false;
    size = static_cast<uint32_t>(rounded);
    return true;
}
bool valid_protection(uint32_t prot) {
    return !(prot & ~(KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE | KU_KERNEL_PROT_EXEC));
}
bool contains_range(const Blocks &blocks, Address addr, uint32_t size) {
    for (const auto &[_, block] : blocks)
        if (addr >= block->mappedBase.address() && uint64_t(addr) + size <= uint64_t(block->mappedBase.address()) + block->mappedSize)
            return true;
    return false;
}
bool owned_range(const SysmemState &state, Address addr, uint32_t size) {
    return contains_range(state.vm_blocks, addr, size) || contains_range(state.blocks, addr, size);
}
KernelMemBlockPtr find_block(const SysmemState &state, SceUID uid) {
    auto it = state.vm_blocks.find(uid);
    if (it != state.vm_blocks.end())
        return it->second;
    it = state.blocks.find(uid);
    return it != state.blocks.end() ? it->second : nullptr;
}
} // namespace

// Convert kubridge protection flags to Vita3K MemPerm.
// EXEC is not enforced here; prefetch abort dispatch is not implemented.
static MemPerm ku_prot_to_memperm(uint32_t prot) {
    if (prot & KU_KERNEL_PROT_WRITE)
        return MemPerm::ReadWrite;
    if (prot & KU_KERNEL_PROT_READ)
        return MemPerm::ReadOnly;
    return MemPerm::None;
}

// Software permissions are per guest alias, never mprotect() of its backing.
EXPORT(int, kuKernelMemProtect, Ptr<void> addr, SceSize len, uint32_t prot) {
    TRACY_FUNC(kuKernelMemProtect, addr, len, prot);
    if (!len)
        return 0;
    uint32_t size;
    if (!normalize_range(addr.address(), len, size) || !valid_protection(prot))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    {
        const std::lock_guard guard(state->mutex);
        if (!owned_range(*state, addr.address(), size) || !protect_guest_memory(emuenv.mem, addr.address(), size, ku_prot_to_memperm(prot)))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }
    emuenv.kernel.invalidate_jit_cache(addr.address(), size);
    return 0;
}

// kuKernelMemReserve — reserve virtual address space.
// On real Vita, this allocates virtual address space without physical backing.
// In Vita3K, we allocate real memory immediately (no lazy allocation).
EXPORT(SceUID, kuKernelMemReserve, Ptr<Ptr<void>> addr, SceSize size, SceKernelMemBlockType memBlockType) {
    TRACY_FUNC(kuKernelMemReserve, addr, size, memBlockType);
    Address requested_addr = 0;
    Address fault;
    if (!addr || !size || (size & 4095) || !access_guest_memory(emuenv.mem, addr.address(), &requested_addr, sizeof(requested_addr), false, fault) || !check_guest_memory_access(emuenv.mem, addr.address(), sizeof(requested_addr), true, fault))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    auto &mem = emuenv.mem;
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    std::unique_lock guard(state->mutex);

    uint32_t checked_size;
    if (requested_addr && !normalize_range(requested_addr, size, checked_size))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    Address allocated_addr;

    if (requested_addr) {
        allocated_addr = alloc_at(mem, requested_addr, size, "kubridge_reserve");
    } else {
        allocated_addr = alloc(mem, size, "kubridge_reserve");
    }

    if (!allocated_addr)
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);

    // Store the allocated address back
    if (!access_guest_memory(mem, addr.address(), &allocated_addr, sizeof(allocated_addr), true, fault)) {
        free(mem, allocated_addr);
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }

    // Track as a vm_block
    const SceUID uid = state->get_next_uid();
    auto block = std::make_shared<KernelMemBlock>();
    block->type = memBlockType;
    block->mappedBase = Ptr<void>(allocated_addr);
    block->mappedSize = size;
    block->size = sizeof(SceKernelMemBlockInfo);
    std::strncpy(block->name, "kubridge_reserve", KERNELOBJECT_MAX_NAME_LENGTH);
    state->vm_blocks.emplace(uid, block);
    // Reservation is initially inaccessible, unlike an ordinary allocation.
    decommit_guest_memory(mem, allocated_addr, size);
    guard.unlock();
    emuenv.kernel.invalidate_jit_cache(allocated_addr, size);

    LOG_DEBUG("kuKernelMemReserve: addr=0x{:08X} size={} type=0x{:08X} uid=0x{:08X}",
        allocated_addr, size, (uint32_t)memBlockType, uid);
    return uid;
}

// HAS_BASE maps shared backing, including offsets, multiple aliases and remaps.
EXPORT(int, kuKernelMemCommit, Ptr<void> addr, SceSize len, uint32_t prot, Ptr<KuKernelMemCommitOpt> pOpt) {
    TRACY_FUNC(kuKernelMemCommit, addr, len, prot, pOpt);
    if (!len)
        return 0;
    uint32_t size;
    if (!normalize_range(addr.address(), len, size) || !valid_protection(prot))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    KuKernelMemCommitOpt opt{};
    if (pOpt) {
        Address fault;
        if (!access_guest_memory(emuenv.mem, pOpt.address(), &opt, sizeof(opt), false, fault))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
        if (opt.size != sizeof(opt))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    }
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    {
        const std::lock_guard guard(state->mutex);
        if (!owned_range(*state, addr.address(), size))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
        bool result;
        if (opt.attr & KU_KERNEL_MEM_COMMIT_ATTR_HAS_BASE) {
            const auto base = find_block(*state, opt.baseBlock);
            if (!base)
                return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID);
            if ((opt.baseOffset & 4095) || opt.baseOffset > base->mappedSize || size > base->mappedSize - opt.baseOffset || uint64_t(base->mappedBase.address()) + opt.baseOffset + size > (uint64_t{ 1 } << 32))
                return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
            result = map_guest_memory(emuenv.mem, addr.address(), size,
                base->mappedBase.address() + opt.baseOffset, ku_prot_to_memperm(prot));
        } else {
            result = commit_guest_memory(emuenv.mem, addr.address(), size, ku_prot_to_memperm(prot));
        }
        if (!result)
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }
    emuenv.kernel.invalidate_jit_cache(addr.address(), size);
    return 0;
}

EXPORT(int, kuKernelMemDecommit, Ptr<void> addr, SceSize len) {
    TRACY_FUNC(kuKernelMemDecommit, addr, len);
    if (!len)
        return 0;
    uint32_t size;
    if (!normalize_range(addr.address(), len, size))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    {
        const std::lock_guard guard(state->mutex);
        if (!owned_range(*state, addr.address(), size) || !decommit_guest_memory(emuenv.mem, addr.address(), size))
            return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    }
    emuenv.kernel.invalidate_jit_cache(addr.address(), size);
    return 0;
}

// kuKernelRegisterExceptionHandler — register user-mode exception handler.
// On real Vita, kubridge hooks ARM exception vectors (DABT/PABT/UNDEF).
// In Vita3K, we store the handler address and invoke it from run_loop
// when a checked guest memory access triggers a data abort.
EXPORT(int, kuKernelRegisterExceptionHandler, uint32_t exceptionType,
    Ptr<void> pHandler, Ptr<Ptr<void>> pOldHandler, Ptr<KuKernelExceptionHandlerOpt> pOpt) {
    TRACY_FUNC(kuKernelRegisterExceptionHandler, exceptionType, pHandler, pOldHandler, pOpt);
    if (exceptionType >= KernelState::EXCEPTION_HANDLER_MAX)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    // Only DABT (type 0) is currently dispatched from run_loop.
    // PABT (1) and UNDEF (2) are stored but never fired.
    if (exceptionType != 0)
        LOG_WARN("kuKernelRegisterExceptionHandler: type={} stored but not yet dispatched (only DABT=0 is)",
            exceptionType);

    const Address old_handler = emuenv.kernel.exception_handlers[exceptionType].exchange(pHandler.address());

    if (pOldHandler) {
        *pOldHandler.get(emuenv.mem) = Ptr<void>(old_handler);
    }

    LOG_INFO("kuKernelRegisterExceptionHandler: type={} handler=0x{:08X} old=0x{:08X}",
        exceptionType, pHandler.address(), old_handler);
    return 0;
}

// kuKernelReleaseExceptionHandler — unregister exception handler.
EXPORT(void, kuKernelReleaseExceptionHandler, uint32_t exceptionType) {
    TRACY_FUNC(kuKernelReleaseExceptionHandler, exceptionType);
    if (exceptionType < KernelState::EXCEPTION_HANDLER_MAX) {
        const Address old = emuenv.kernel.exception_handlers[exceptionType].exchange(0);
        LOG_INFO("kuKernelReleaseExceptionHandler: type={} old=0x{:08X}", exceptionType, old);
    }
}
