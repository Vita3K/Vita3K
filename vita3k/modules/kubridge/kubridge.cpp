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

// Page fault callback that triggers an abort handler if one is registered.
// Called from signal handler context — must be signal-safe.
// Sets abort_pending on the current thread's CPU and halts execution so
// run_loop can invoke the ARM exception handler.
static bool abort_aware_fault_callback(Address fault_addr, bool write, Address range_start, uint32_t range_size) {
    CPUState *cpu = get_current_cpu_state();
    if (cpu) {
        cpu->abort_fault_addr.store(fault_addr);
        cpu->abort_is_write.store(write);
        cpu->abort_pending.store(true);
        stop(*cpu); // HaltExecution — signal-safe (atomic only)
    }
    return true;
}

// Convert kubridge protection flags to Vita3K MemPerm.
// Note: EXEC is not mapped — dynarmic handles execute permission separately.
static MemPerm ku_prot_to_memperm(uint32_t prot) {
    if (prot & KU_KERNEL_PROT_WRITE)
        return MemPerm::ReadWrite;
    if (prot & KU_KERNEL_PROT_READ)
        return MemPerm::ReadOnly;
    return MemPerm::None;
}

// kuKernelMemProtect — change memory protection on a range.
// Uses add_protect for restrictive permissions (so fault handler can catch access),
// and unprotect_inner to restore access.
EXPORT(int, kuKernelMemProtect, Ptr<void> addr, SceSize len, uint32_t prot) {
    TRACY_FUNC(kuKernelMemProtect, addr, len, prot);
    if (!addr || len == 0)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    const MemPerm perm = ku_prot_to_memperm(prot);

    if (perm == MemPerm::None || perm == MemPerm::ReadOnly) {
        // Use add_protect so fault handler can catch violations gracefully.
        // If an exception handler is registered, the fault callback will
        // halt the CPU and let run_loop invoke the ARM handler.
        const Address va = addr.address();
        add_protect(emuenv.mem, va, len, perm, [va, len](Address fault_addr, bool write) {
            return abort_aware_fault_callback(fault_addr, write, va, len);
        });
    } else {
        // ReadWrite or more permissive — just unprotect
        unprotect_inner(emuenv.mem, addr.address(), len);
    }

    LOG_DEBUG("kuKernelMemProtect: addr={} len={} prot=0x{:02X} -> perm={}",
        log_hex(addr.address()), len, prot, (int)perm);
    return 0;
}

// kuKernelMemReserve — reserve virtual address space.
// On real Vita, this allocates virtual address space without physical backing.
// In Vita3K, we allocate real memory immediately (no lazy allocation).
EXPORT(SceUID, kuKernelMemReserve, Ptr<Ptr<void>> addr, SceSize size, SceKernelMemBlockType memBlockType) {
    TRACY_FUNC(kuKernelMemReserve, addr, size, memBlockType);
    if (!addr)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    auto &mem = emuenv.mem;
    const auto state = emuenv.kernel.obj_store.get<SysmemState>();
    const auto guard = std::lock_guard<std::mutex>(state->mutex);

    Address requested_addr = (*addr.get(mem)).address();
    Address allocated_addr;

    if (requested_addr) {
        allocated_addr = alloc_at(mem, requested_addr, size, "kubridge_reserve");
    } else {
        allocated_addr = alloc(mem, size, "kubridge_reserve");
    }

    if (!allocated_addr)
        return RET_ERROR(SCE_KERNEL_ERROR_NO_MEMORY);

    // Store the allocated address back
    *addr.get(mem) = Ptr<void>(allocated_addr);

    // Track as a vm_block
    const SceUID uid = state->get_next_uid();
    auto block = std::make_shared<KernelMemBlock>();
    block->type = memBlockType;
    block->mappedBase = Ptr<void>(allocated_addr);
    block->mappedSize = size;
    block->size = sizeof(SceKernelMemBlockInfo);
    std::strncpy(block->name, "kubridge_reserve", KERNELOBJECT_MAX_NAME_LENGTH);
    state->vm_blocks.emplace(uid, block);

    LOG_DEBUG("kuKernelMemReserve: addr=0x{:08X} size={} type=0x{:08X} uid=0x{:08X}",
        allocated_addr, size, (uint32_t)memBlockType, uid);
    return uid;
}

// kuKernelMemCommit — commit physical pages to a reserved range.
// Memory is already committed at Reserve time. Apply requested protection.
// When baseBlock mirroring is required, this should use add_external_mapping().
EXPORT(int, kuKernelMemCommit, Ptr<void> addr, SceSize len, uint32_t prot, Ptr<KuKernelMemCommitOpt> pOpt) {
    TRACY_FUNC(kuKernelMemCommit, addr, len, prot, pOpt);
    if (!addr || len == 0)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    if (pOpt) {
        auto *opt = pOpt.get(emuenv.mem);
        if (opt->attr & KU_KERNEL_MEM_COMMIT_ATTR_HAS_BASE) {
            LOG_WARN("kuKernelMemCommit: baseBlock mirrors not yet implemented (baseBlock=0x{:08X} offset=0x{:08X})",
                opt->baseBlock, opt->baseOffset);
        }
    }

    const MemPerm perm = ku_prot_to_memperm(prot);
    const Address va = addr.address();

    if (perm == MemPerm::None || perm == MemPerm::ReadOnly) {
        add_protect(emuenv.mem, va, len, perm, [va, len](Address fault_addr, bool write) {
            return abort_aware_fault_callback(fault_addr, write, va, len);
        });
    } else {
        unprotect_inner(emuenv.mem, va, len);
    }

    LOG_DEBUG("kuKernelMemCommit: addr={} len={} prot=0x{:02X} -> perm={}",
        log_hex(va), len, prot, (int)perm);
    return 0;
}

// kuKernelMemDecommit — release physical pages from a committed range.
// Uses add_protect so the fault handler catches accesses gracefully
// instead of crashing the emulator.
EXPORT(int, kuKernelMemDecommit, Ptr<void> addr, SceSize len) {
    TRACY_FUNC(kuKernelMemDecommit, addr, len);
    if (!addr || len == 0)
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);

    const Address va = addr.address();
    add_protect(emuenv.mem, va, len, MemPerm::None, [va, len](Address fault_addr, bool write) {
        return abort_aware_fault_callback(fault_addr, write, va, len);
    });

    LOG_DEBUG("kuKernelMemDecommit: addr={} len={}", log_hex(va), len);
    return 0;
}

// kuKernelRegisterExceptionHandler — register user-mode exception handler.
// On real Vita, kubridge hooks ARM exception vectors (DABT/PABT/UNDEF).
// In Vita3K, we store the handler address and invoke it from run_loop
// when a page fault triggers an abort via the add_protect callback.
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
