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

// kubridge — kernel bridge plugin for PlayStation Vita
// https://github.com/TheOfficialFloW/kubridge
//
// kubridge.skprx is a kernel plugin that exposes privileged kernel functions
// to user-mode applications. The functions are exported by the "KuBridge"
// library inside the "kubridge" module.

#pragma once

#include <module/module.h>
#include <kernel/types.h>

// ------------------------------------------------------------------
// kuKernelLoad
// Load a kernel or user module from the given path.
DECL_EXPORT(SceUID, kuKernelLoad, const char *path, const SceKernelLMOption *option);

// ------------------------------------------------------------------
// kuKernelCall
// Call a guest function with the given arguments.
// In user-mode emulation we run the function in a new guest thread.
// argc = number of additional 32-bit words passed after func_addr;
// they are read from the ARM argument registers / stack of the caller.
DECL_EXPORT(int, kuKernelCall, Ptr<const void> func_addr, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

// ------------------------------------------------------------------
// kuKernelCpuUnrestrictedMemcpy
// Copy memory without address-space restriction checks.
// In emulation this is a direct guest-memory memcpy.
DECL_EXPORT(int, kuKernelCpuUnrestrictedMemcpy, Ptr<void> dst, Ptr<const void> src, SceSize size);

// ------------------------------------------------------------------
// kuKernelGetModel
// Returns the PSVita hardware model (SCE_KERNEL_MODEL_VITA / SCE_KERNEL_MODEL_VITATV).
DECL_EXPORT(int, kuKernelGetModel);

// ------------------------------------------------------------------
// kuKernelFlushCaches
// Flush/invalidate CPU caches for the given guest address range.
// In emulation, we invalidate the JIT cache for the range.
DECL_EXPORT(int, kuKernelFlushCaches, Ptr<void> ptr, SceSize size);

// ------------------------------------------------------------------
// kuKernelIcacheInvalidateRange
// Invalidate instruction cache for the given guest address range.
DECL_EXPORT(int, kuKernelIcacheInvalidateRange, Ptr<void> ptr, SceSize size);

// ------------------------------------------------------------------
// kuKernelDcacheWritebackInvalidateRange
// Writeback + invalidate data cache for the given guest address range.
// In emulation this is a no-op (all writes go directly to guest memory).
DECL_EXPORT(int, kuKernelDcacheWritebackInvalidateRange, Ptr<void> ptr, SceSize size);

// ------------------------------------------------------------------
// kuKernelLoadStartModule
// Load and start a user/kernel module.
DECL_EXPORT(SceUID, kuKernelLoadStartModule, const char *path, SceSize args, Ptr<const void> argp, int flags, const SceKernelLMOption *opt, int *res);

// ------------------------------------------------------------------
// kuKernelStopUnloadModule
// Stop and unload a module.
DECL_EXPORT(int, kuKernelStopUnloadModule, SceUID uid, SceSize args, Ptr<const void> argp, int flags, const void *opt, int *res);

// ------------------------------------------------------------------
// kuKernelGetModule
// Search loaded modules by name and fill *info.
DECL_EXPORT(int, kuKernelGetModuleInfo, SceUID uid, SceKernelModuleInfo *info);

// ------------------------------------------------------------------
// kuKernelFillMem
// Fill a guest memory region with a pattern byte.
DECL_EXPORT(int, kuKernelFillMem, Ptr<void> dst, int pattern, SceSize size);

// ------------------------------------------------------------------
// kuKernelRemap
// Remap memory block permissions (stub – memory protection is not
// enforced inside the emulator).
DECL_EXPORT(int, kuKernelRemap, Ptr<void> dst, SceSize size, uint32_t perm);

// ------------------------------------------------------------------
// kuKernelGetProcInfo
// Return process info (stub).
DECL_EXPORT(int, kuKernelGetProcInfo, Ptr<void> info);
