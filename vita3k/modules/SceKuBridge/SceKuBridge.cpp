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
// kubridge (https://github.com/TheOfficialFloW/kubridge) is a kernel bridge
// plugin for the PlayStation Vita. It allows user-mode code to call a small
// set of privileged kernel functions. This file implements those functions
// entirely in host (HLE) code so that games and plugins that depend on
// kubridge can run without an actual kernel module.
//
// The module name as seen by the guest is "kubridge" and the library is
// "KuBridge". When a game performs taiLoadStartModule("ur0:tai/kubridge.skprx")
// (or similar), Vita3K's SceTaiHen module returns TAI_ERROR_NOT_IMPLEMENTED
// because we cannot load real kernel modules. However, the kubridge functions
// are registered directly into export_nids here, so any import stubs that
// reference the KuBridge NIDs are resolved before the module is loaded.

#include "SceKuBridge.h"

#include <kernel/load_self.h>
#include <kernel/state.h>
#include <modules/module_parent.h>
#include <util/lock_and_find.h>
#include <util/log.h>
#include <util/tracy.h>

#include <cstring>

TRACY_MODULE_NAME(SceKuBridge);

// ---------------------------------------------------------------------------
// kuKernelLoad
// Load a module. In Vita3K we delegate to load_module() which handles both
// user and "kernel" paths by mapping them inside the guest address space.
EXPORT(SceUID, kuKernelLoad, const char *path, const SceKernelLMOption *option) {
    TRACY_FUNC(kuKernelLoad, path, option);
    if (!path) {
        LOG_ERROR("kuKernelLoad: null path");
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    LOG_INFO("kuKernelLoad: \"{}\"", path);
    return load_module(emuenv, path);
}

// ---------------------------------------------------------------------------
// kuKernelCall
// Execute a guest function with up to four 32-bit arguments.
// The real kubridge dispatches this via a kernel supervisor call chain.
// In user-mode emulation we build a small ARM trampoline that pre-loads R0-R3
// from embedded immediate values and then jumps to the target function, so
// the function receives the correct arguments.
EXPORT(int, kuKernelCall, Ptr<const void> func_addr, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    TRACY_FUNC(kuKernelCall, func_addr, arg0, arg1, arg2, arg3);
    if (!func_addr) {
        LOG_ERROR("kuKernelCall: null function address");
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    LOG_INFO("kuKernelCall: func={:#010x} args=({:#010x},{:#010x},{:#010x},{:#010x})",
        func_addr.address(), arg0, arg1, arg2, arg3);

    // Build an ARM trampoline that loads R0-R3 from MOVW/MOVT pairs and
    // then jumps to func_addr using LDR PC, [PC, #-4].
    //
    // Layout (10 × 4 bytes = 40 bytes):
    //   [0]  MOVW R0, #(arg0 & 0xFFFF)
    //   [1]  MOVT R0, #(arg0 >> 16)
    //   [2]  MOVW R1, #(arg1 & 0xFFFF)
    //   [3]  MOVT R1, #(arg1 >> 16)
    //   [4]  MOVW R2, #(arg2 & 0xFFFF)
    //   [5]  MOVT R2, #(arg2 >> 16)
    //   [6]  MOVW R3, #(arg3 & 0xFFFF)
    //   [7]  MOVT R3, #(arg3 >> 16)
    //   [8]  LDR  PC, [PC, #-4]   ; when at [8], PC = [8]+8 = [10], so loads [9]
    //   [9]  func_addr

    auto arm_movw = [](uint32_t reg, uint32_t imm16) -> uint32_t {
        return (0xE30u << 20) | ((imm16 & 0xF000u) << 4) | (imm16 & 0xFFFu) | (reg << 12);
    };
    auto arm_movt = [](uint32_t reg, uint32_t imm16) -> uint32_t {
        return (0xE34u << 20) | ((imm16 & 0xF000u) << 4) | (imm16 & 0xFFFu) | (reg << 12);
    };
    constexpr uint32_t LDR_PC_PC_M4 = 0xE51FF004u;
    constexpr uint32_t TRAMPOLINE_WORDS = 10;

    const Address tramp_addr = alloc(emuenv.mem, TRAMPOLINE_WORDS * 4, "kuKernelCall trampoline");
    if (!tramp_addr) {
        LOG_ERROR("kuKernelCall: failed to allocate trampoline");
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    uint32_t *tramp = Ptr<uint32_t>(tramp_addr).get(emuenv.mem);
    uint32_t i = 0;
    tramp[i++] = arm_movw(0, arg0 & 0xFFFFu);
    tramp[i++] = arm_movt(0, (arg0 >> 16) & 0xFFFFu);
    tramp[i++] = arm_movw(1, arg1 & 0xFFFFu);
    tramp[i++] = arm_movt(1, (arg1 >> 16) & 0xFFFFu);
    tramp[i++] = arm_movw(2, arg2 & 0xFFFFu);
    tramp[i++] = arm_movt(2, (arg2 >> 16) & 0xFFFFu);
    tramp[i++] = arm_movw(3, arg3 & 0xFFFFu);
    tramp[i++] = arm_movt(3, (arg3 >> 16) & 0xFFFFu);
    tramp[i++] = LDR_PC_PC_M4;
    tramp[i++] = func_addr.address();
    emuenv.kernel.invalidate_jit_cache(tramp_addr, TRAMPOLINE_WORDS * 4);

    const ThreadStatePtr thread = emuenv.kernel.create_thread(emuenv.mem, "kuKernelCall worker",
        Ptr<const void>(tramp_addr),
        SCE_KERNEL_DEFAULT_PRIORITY_USER,
        SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT,
        SCE_KERNEL_STACK_SIZE_USER_MAIN, nullptr);

    int result = 0;
    if (thread) {
        // Run with args=0 so start() sets R0=0/R1=0; the trampoline immediately
        // overwrites R0-R3 with the correct values via MOVW/MOVT.
        result = static_cast<int>(thread->run_guest_function(tramp_addr, 0, Ptr<void>{}));
        thread->exit_delete(false);
    } else {
        LOG_ERROR("kuKernelCall: could not create worker thread");
        result = SCE_KERNEL_ERROR_NO_MEMORY;
    }

    free(emuenv.mem, tramp_addr);
    return result;
}

// ---------------------------------------------------------------------------
// kuKernelCpuUnrestrictedMemcpy
// Copy between any two guest addresses without protection checks.
EXPORT(int, kuKernelCpuUnrestrictedMemcpy, Ptr<void> dst, Ptr<const void> src, SceSize size) {
    TRACY_FUNC(kuKernelCpuUnrestrictedMemcpy, dst, src, size);
    if (!dst || !src || size == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    void *host_dst = dst.get(emuenv.mem);
    const void *host_src = src.get(emuenv.mem);
    if (!host_dst || !host_src) {
        LOG_ERROR("kuKernelCpuUnrestrictedMemcpy: invalid guest pointer");
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    std::memmove(host_dst, host_src, size);
    // Invalidate the JIT cache for the destination range in case it contains code.
    emuenv.kernel.invalidate_jit_cache(dst.address(), size);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelGetModel
// Return the PSVita hardware model.
EXPORT(int, kuKernelGetModel) {
    TRACY_FUNC(kuKernelGetModel);
    return emuenv.cfg.current_config.pstv_mode ? SCE_KERNEL_MODEL_VITATV : SCE_KERNEL_MODEL_VITA;
}

// ---------------------------------------------------------------------------
// kuKernelFlushCaches
// Flush instruction and data caches. In emulation this triggers a JIT cache
// invalidation so the recompiler picks up any new code written to memory.
EXPORT(int, kuKernelFlushCaches, Ptr<void> ptr, SceSize size) {
    TRACY_FUNC(kuKernelFlushCaches, ptr, size);
    if (ptr && size > 0)
        emuenv.kernel.invalidate_jit_cache(ptr.address(), size);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelIcacheInvalidateRange
// Invalidate instruction cache for the given range.
EXPORT(int, kuKernelIcacheInvalidateRange, Ptr<void> ptr, SceSize size) {
    TRACY_FUNC(kuKernelIcacheInvalidateRange, ptr, size);
    if (ptr && size > 0)
        emuenv.kernel.invalidate_jit_cache(ptr.address(), size);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelDcacheWritebackInvalidateRange
// Writeback + invalidate data cache. In emulation writes are always coherent,
// so this is effectively a no-op (but we still invalidate the JIT cache).
EXPORT(int, kuKernelDcacheWritebackInvalidateRange, Ptr<void> ptr, SceSize size) {
    TRACY_FUNC(kuKernelDcacheWritebackInvalidateRange, ptr, size);
    if (ptr && size > 0)
        emuenv.kernel.invalidate_jit_cache(ptr.address(), size);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelLoadStartModule
// Load and start a module. Delegates to the same helpers as SceTaiHen and
// SceModulemgr.
EXPORT(SceUID, kuKernelLoadStartModule, const char *path, SceSize args, Ptr<const void> argp, int flags, const SceKernelLMOption *opt, int *res) {
    TRACY_FUNC(kuKernelLoadStartModule, path, args, argp, flags, opt, res);
    if (!path) {
        LOG_ERROR("kuKernelLoadStartModule: null path");
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    LOG_INFO("kuKernelLoadStartModule: \"{}\"", path);
    const SceUID module_id = load_module(emuenv, path);
    if (module_id < 0) {
        LOG_ERROR("kuKernelLoadStartModule: load_module failed ({:#010x})", module_id);
        return module_id;
    }
    const SceKernelModulePtr module = lock_and_find(module_id, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module)
        return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
    const int start_result = static_cast<int>(start_module(emuenv, module->info, args, argp));
    if (res)
        *res = start_result;
    return module_id;
}

// ---------------------------------------------------------------------------
// kuKernelStopUnloadModule
// Stop and unload a module.
EXPORT(int, kuKernelStopUnloadModule, SceUID uid, SceSize args, Ptr<const void> argp, int flags, const void *opt, int *res) {
    TRACY_FUNC(kuKernelStopUnloadModule, uid, args, argp, flags, opt, res);
    const SceKernelModulePtr module = lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        LOG_ERROR("kuKernelStopUnloadModule: module {} not found", uid);
        return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
    }
    const int stop_result = static_cast<int>(stop_module(emuenv, module->info, args, argp));
    if (res)
        *res = stop_result;
    return unload_module(emuenv, uid);
}

// ---------------------------------------------------------------------------
// kuKernelGetModuleInfo
// Return the SceKernelModuleInfo for the module with the given UID.
EXPORT(int, kuKernelGetModuleInfo, SceUID uid, SceKernelModuleInfo *info) {
    TRACY_FUNC(kuKernelGetModuleInfo, uid, info);
    if (!info)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    const SceKernelModulePtr module = lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        LOG_WARN("kuKernelGetModuleInfo: module {} not found", uid);
        return SCE_KERNEL_ERROR_MODULEMGR_NO_MOD;
    }
    if (info->size < sizeof(SceKernelModuleInfo)) {
        LOG_ERROR("kuKernelGetModuleInfo: info->size ({}) < expected ({})", info->size, sizeof(SceKernelModuleInfo));
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    // Preserve the caller's size field; fill the rest from module->info.
    const SceSize caller_size = info->size;
    std::memcpy(info, &module->info, sizeof(SceKernelModuleInfo));
    info->size = caller_size;
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelFillMem
// Fill a guest memory block with a repeated byte pattern.
EXPORT(int, kuKernelFillMem, Ptr<void> dst, int pattern, SceSize size) {
    TRACY_FUNC(kuKernelFillMem, dst, pattern, size);
    if (!dst || size == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    void *host_dst = dst.get(emuenv.mem);
    if (!host_dst)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    std::memset(host_dst, pattern & 0xFF, size);
    emuenv.kernel.invalidate_jit_cache(dst.address(), size);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelRemap
// Change memory block permissions. Vita3K does not enforce page permissions in
// guest space (all memory is mapped RWX from the host perspective), so this
// is a no-op that returns success.
EXPORT(int, kuKernelRemap, Ptr<void> dst, SceSize size, uint32_t perm) {
    TRACY_FUNC(kuKernelRemap, dst, size, perm);
    LOG_DEBUG("kuKernelRemap: {:#010x} size={} perm={:#x} (no-op)", dst.address(), size, perm);
    return SCE_KERNEL_OK;
}

// ---------------------------------------------------------------------------
// kuKernelGetProcInfo
// Return process info (stub – not needed for typical use cases).
EXPORT(int, kuKernelGetProcInfo, Ptr<void> info) {
    TRACY_FUNC(kuKernelGetProcInfo, info);
    return UNIMPLEMENTED();
}
