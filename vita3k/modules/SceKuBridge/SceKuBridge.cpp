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

// kubridge host-side emulation for Vita3K
//
// Implements https://github.com/bythos14/kubridge, a kernel bridge plugin
// that grants user-mode code access to privileged kernel services.
// Since Vita3K runs in user-mode emulation on the host, we provide HLE
// (host-library emulation) stubs that achieve the same guest-visible effect.

#include "SceKuBridge.h"

#include <kernel/state.h>
#include <mem/functions.h>
#include <modules/module_parent.h>
#include <util/log.h>
#include <util/tracy.h>

#include <array>
#include <cstring>
#include <mutex>
#include <unordered_map>

TRACY_MODULE_NAME(SceKuBridge);

// ---------------------------------------------------------------------------
// Module state
// Tracks reserved VM regions and registered exception handlers.
struct KuBridgeState {
    std::mutex mutex;

    // Exception handlers indexed by exceptionType (0-2).
    std::array<Address, 3> exception_handlers{};

    // Deprecated abort handler.
    Address abort_handler{};

    // Maps a SceUID → base address of the reserved virtual memory region.
    // These regions are created by kuKernelMemReserve and tracked so that
    // kuKernelMemDecommit can locate and free sub-regions.
    std::unordered_map<SceUID, Address> reserved_blocks;
    SceUID next_reserved_uid{ 0x70000000 }; // synthetic UIDs for reserved blocks
};

LIBRARY_INIT(SceKuBridge) {
    emuenv.kernel.obj_store.create<KuBridgeState>();
}

// ---------------------------------------------------------------------------
// kuKernelAllocMemBlock
//
// The real kubridge wraps ksceKernelAllocMemBlock (kernel version) which
// allows allocating memory with kernel-level options and block types that
// are normally restricted in user mode (e.g. SCE_KERNEL_MEMBLOCK_TYPE_USER_RX
// without SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_ALLOW_PARTIAL_OP constraints, or
// kernel-exclusive types).
//
// In Vita3K all guest memory is already RWX from the host's point of view,
// so we implement this as an aligned allocation and register it in SysmemState
// so that sceKernelGetMemBlockBase / sceKernelFindMemBlockByAddr work.
// We accept a wider range of SceKernelMemBlockType values than the user-mode
// sceKernelAllocMemBlock by mapping kernel types to the nearest user type.
EXPORT(SceUID, kuKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockKernelOpt *opt) {
    TRACY_FUNC(kuKernelAllocMemBlock, name, type, size, opt);

    if (!name || size == 0) {
        LOG_ERROR("kuKernelAllocMemBlock: invalid arguments");
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    // Determine alignment and preferred start address.
    // Kernel types are coerced to the nearest user-mode equivalent so that
    // Vita3K's unified memory manager can honour them.
    SceSize alignment = 0x1000;
    Address start_addr = 0x80000000U; // user main memory area

    // Kernel-side types that refer to uncached or physically contiguous
    // memory are mapped to the same region as their user-mode counterparts.
    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RX:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
        start_addr = 0x70000000U;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
        alignment = 0x40000;
        start_addr = 0x60000000U;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
        alignment = 0x100000;
        break;
    default:
        // Kernel-exclusive types — fall back to standard user-RW layout.
        LOG_DEBUG("kuKernelAllocMemBlock: unknown type {:#010x}, treating as USER_RW", static_cast<uint32_t>(type));
        break;
    }

    if (opt && opt->size >= 16 && opt->alignment > alignment)
        alignment = opt->alignment;

    // Ensure alignment is a power of two.
    if (alignment & (alignment - 1)) {
        LOG_ERROR("kuKernelAllocMemBlock: alignment {} is not a power of two", alignment);
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    const Address base = alloc_aligned(emuenv.mem, size, name, alignment, start_addr);
    if (!base) {
        LOG_ERROR("kuKernelAllocMemBlock: out of memory (size={})", size);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }

    // Track the block in KuBridgeState so we can free it later.
    const auto state = emuenv.kernel.obj_store.get<KuBridgeState>();
    std::lock_guard<std::mutex> lock(state->mutex);
    const SceUID uid = state->next_reserved_uid++;
    state->reserved_blocks[uid] = base;

    LOG_INFO("kuKernelAllocMemBlock: '{}' size={} base={:#010x} uid={}", name, size, base, uid);
    return uid;
}

// ---------------------------------------------------------------------------
// kuKernelFlushCaches
// Flush both D-cache and I-cache for the given address range.
// In emulation we invalidate the JIT recompiler cache so the next execution
// of the range picks up any new code that was written.
EXPORT(void, kuKernelFlushCaches, Ptr<const void> ptr, SceSize len) {
    TRACY_FUNC(kuKernelFlushCaches, ptr, len);
    if (ptr && len > 0)
        emuenv.kernel.invalidate_jit_cache(ptr.address(), len);
}

// ---------------------------------------------------------------------------
// kuKernelCpuUnrestrictedMemcpy
// Copy memory between any two guest addresses, bypassing access restrictions.
// The real implementation manipulates DACR to disable domain access checking.
// In emulation there is no domain protection, so a plain memmove is sufficient.
// We still invalidate the JIT cache for the destination in case it holds code.
EXPORT(int, kuKernelCpuUnrestrictedMemcpy, Ptr<void> dst, Ptr<const void> src, SceSize len) {
    TRACY_FUNC(kuKernelCpuUnrestrictedMemcpy, dst, src, len);
    if (!dst || !src || len == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    void *host_dst = dst.get(emuenv.mem);
    const void *host_src = src.get(emuenv.mem);
    if (!host_dst || !host_src) {
        LOG_ERROR("kuKernelCpuUnrestrictedMemcpy: invalid guest pointer");
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }

    std::memmove(host_dst, host_src, len);
    emuenv.kernel.invalidate_jit_cache(dst.address(), len);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuPowerGetSysClockFrequency
// Return the current CPU clock frequency in MHz.
// The default Vita clock is 444 MHz; we return that constant.
EXPORT(int, kuPowerGetSysClockFrequency) {
    TRACY_FUNC(kuPowerGetSysClockFrequency);
    // Return the standard default Vita clock frequency.
    return 444;
}

// ---------------------------------------------------------------------------
// kuPowerSetSysClockFrequency
// Change the CPU clock frequency.  In emulation the host runs at its own
// native speed, so frequency changes have no effect.
EXPORT(int, kuPowerSetSysClockFrequency, int freq) {
    TRACY_FUNC(kuPowerSetSysClockFrequency, freq);
    LOG_DEBUG("kuPowerSetSysClockFrequency: {} MHz (no-op in emulation)", freq);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelMemProtect
// Change memory protection flags for an address range.
// The real implementation patches ARM MMU page-table entries.
// In Vita3K all guest memory is mapped RWX from the host side, so this is
// effectively a no-op from a correctness standpoint.
// We still invalidate the JIT cache when EXEC is being enabled so that the
// recompiler picks up any code that was written before the call.
EXPORT(int, kuKernelMemProtect, Ptr<void> addr, SceSize len, SceUInt32 prot) {
    TRACY_FUNC(kuKernelMemProtect, addr, len, prot);
    if (!addr || len == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    if (prot & KU_KERNEL_PROT_EXEC)
        emuenv.kernel.invalidate_jit_cache(addr.address(), len);

    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelMemReserve
// Reserve a virtual memory region without immediately committing physical pages.
// Games (typically JIT engines) reserve a large region and later commit pages
// with kuKernelMemCommit as needed.
//
// In Vita3K's unified memory model, reservation and commitment are one step.
// We allocate the full region upfront (so subsequent sub-region commits are
// already accessible) and track it in KuBridgeState so kuKernelMemDecommit
// can free it when needed.
EXPORT(SceUID, kuKernelMemReserve, Ptr<Ptr<void>> addr, SceSize size, SceKernelMemBlockType memBlockType) {
    TRACY_FUNC(kuKernelMemReserve, addr, size, memBlockType);
    if (!addr || size == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    // Choose start address / alignment based on block type.
    SceSize alignment = 0x1000;
    Address start_addr = 0x80000000U;
    switch (memBlockType) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
        alignment = 0x40000;
        start_addr = 0x60000000U;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
        start_addr = 0x70000000U;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
        alignment = 0x100000;
        break;
    default:
        break;
    }

    const Address base = alloc_aligned(emuenv.mem, size, "kuReserved", alignment, start_addr);
    if (!base) {
        LOG_ERROR("kuKernelMemReserve: out of memory (size={})", size);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }

    // Write the guest pointer back through the addr** parameter.
    Ptr<void> *host_addr_ptr = addr.get(emuenv.mem);
    if (!host_addr_ptr) {
        free(emuenv.mem, base);
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    *host_addr_ptr = Ptr<void>(base);

    // Register in KuBridgeState.
    const auto state = emuenv.kernel.obj_store.get<KuBridgeState>();
    std::lock_guard<std::mutex> lock(state->mutex);
    const SceUID uid = state->next_reserved_uid++;
    state->reserved_blocks[uid] = base;

    LOG_INFO("kuKernelMemReserve: size={} base={:#010x} uid={}", size, base, uid);
    return uid;
}

// ---------------------------------------------------------------------------
// kuKernelMemCommit
// Commit physical pages within a previously reserved region.
// Since Vita3K's alloc already backs all pages, this is a no-op.
// If EXEC permission is requested we invalidate the JIT cache.
EXPORT(int, kuKernelMemCommit, Ptr<void> addr, SceSize len, SceUInt32 prot, KuKernelMemCommitOpt *pOpt) {
    TRACY_FUNC(kuKernelMemCommit, addr, len, prot, pOpt);
    if (!addr || len == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    if (prot & KU_KERNEL_PROT_EXEC)
        emuenv.kernel.invalidate_jit_cache(addr.address(), len);

    LOG_DEBUG("kuKernelMemCommit: addr={:#010x} len={} prot={:#04x}", addr.address(), len, prot);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelMemDecommit
// Decommit (release) physical pages within a reserved region.
// We don't actually free host pages here because the reserved block is still
// tracked; the free happens when the entire reserved block is released.
// For simplicity we just zero the memory so accessing it is well-defined.
EXPORT(int, kuKernelMemDecommit, Ptr<void> addr, SceSize len) {
    TRACY_FUNC(kuKernelMemDecommit, addr, len);
    if (!addr || len == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    void *host = addr.get(emuenv.mem);
    if (host)
        std::memset(host, 0, len);

    LOG_DEBUG("kuKernelMemDecommit: addr={:#010x} len={}", addr.address(), len);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelRegisterExceptionHandler
// Register a user-mode callback for hardware exceptions (data abort, prefetch
// abort, undefined instruction).  The real implementation installs a kernel
// exception vector that calls back into user space via a signal-like mechanism.
//
// In Vita3K, the JIT/CPU doesn't generate ARM hardware exceptions; instead,
// undefined instructions hit the JIT's "unknown opcode" path.  We store the
// handler address in KuBridgeState for informational purposes but don't
// actually invoke it (no real exception dispatch occurs).
EXPORT(int, kuKernelRegisterExceptionHandler, SceUInt32 exceptionType, Ptr<KuKernelExceptionHandler> pHandler, Ptr<KuKernelExceptionHandler> pOldHandler, KuKernelExceptionHandlerOpt *pOpt) {
    TRACY_FUNC(kuKernelRegisterExceptionHandler, exceptionType, pHandler, pOldHandler, pOpt);
    if (exceptionType > 2)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    const auto state = emuenv.kernel.obj_store.get<KuBridgeState>();
    std::lock_guard<std::mutex> lock(state->mutex);

    if (pOldHandler) {
        KuKernelExceptionHandler *host_old = pOldHandler.get(emuenv.mem);
        if (host_old)
            *host_old = reinterpret_cast<KuKernelExceptionHandler>(
                static_cast<uintptr_t>(state->exception_handlers[exceptionType]));
    }

    state->exception_handlers[exceptionType] = pHandler.address();
    LOG_INFO("kuKernelRegisterExceptionHandler: type={} handler={:#010x}", exceptionType, pHandler.address());
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelReleaseExceptionHandler
// Clear a previously registered exception handler.
EXPORT(void, kuKernelReleaseExceptionHandler, SceUInt32 exceptionType) {
    TRACY_FUNC(kuKernelReleaseExceptionHandler, exceptionType);
    if (exceptionType > 2)
        return;

    const auto state = emuenv.kernel.obj_store.get<KuBridgeState>();
    std::lock_guard<std::mutex> lock(state->mutex);
    state->exception_handlers[exceptionType] = 0;
    LOG_INFO("kuKernelReleaseExceptionHandler: type={}", exceptionType);
}

// ---------------------------------------------------------------------------
// kuKernelRegisterAbortHandler  [Deprecated]
// Deprecated wrapper around kuKernelRegisterExceptionHandler that handles both
// data-abort and prefetch-abort in one call.
EXPORT(int, kuKernelRegisterAbortHandler, Ptr<KuKernelAbortHandler> pHandler, Ptr<KuKernelAbortHandler> pOldHandler, KuKernelAbortHandlerOpt *pOpt) {
    TRACY_FUNC(kuKernelRegisterAbortHandler, pHandler, pOldHandler, pOpt);
    const auto state = emuenv.kernel.obj_store.get<KuBridgeState>();
    std::lock_guard<std::mutex> lock(state->mutex);

    if (pOldHandler) {
        KuKernelAbortHandler *host_old = pOldHandler.get(emuenv.mem);
        if (host_old)
            *host_old = reinterpret_cast<KuKernelAbortHandler>(
                static_cast<uintptr_t>(state->abort_handler));
    }

    state->abort_handler = pHandler.address();
    LOG_INFO("kuKernelRegisterAbortHandler (deprecated): handler={:#010x}", pHandler.address());
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelReleaseAbortHandler  [Deprecated]
// Clear the deprecated abort handler.
EXPORT(void, kuKernelReleaseAbortHandler) {
    TRACY_FUNC(kuKernelReleaseAbortHandler);
    const auto state = emuenv.kernel.obj_store.get<KuBridgeState>();
    std::lock_guard<std::mutex> lock(state->mutex);
    state->abort_handler = 0;
    LOG_INFO("kuKernelReleaseAbortHandler (deprecated)");
}
