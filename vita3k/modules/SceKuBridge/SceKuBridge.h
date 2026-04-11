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
// https://github.com/bythos14/kubridge
//
// kubridge.skprx is a kernel plugin that exposes privileged kernel functions
// to user-mode applications. The functions are exported by the "kubridge"
// library inside the "kubridge" module.

#pragma once

#include <module/module.h>
#include <kernel/types.h>

// Protection flags (KU_KERNEL_PROT_*)
#define KU_KERNEL_PROT_NONE  (0x00)
#define KU_KERNEL_PROT_READ  (0x40)
#define KU_KERNEL_PROT_WRITE (0x20)
#define KU_KERNEL_PROT_EXEC  (0x10)

// Attributes for KuKernelMemCommitOpt
#define KU_KERNEL_MEM_COMMIT_ATTR_HAS_BASE (0x1)

// Exception types
#define KU_KERNEL_EXCEPTION_TYPE_DATA_ABORT            0
#define KU_KERNEL_EXCEPTION_TYPE_PREFETCH_ABORT        1
#define KU_KERNEL_EXCEPTION_TYPE_UNDEFINED_INSTRUCTION 2

// Guest-side exception context (ARM register state at fault time)
struct KuKernelExceptionContext {
    SceUInt32 r0, r1, r2, r3, r4, r5, r6, r7;
    SceUInt32 r8, r9, r10, r11, r12;
    SceUInt32 sp, lr, pc;
    SceUInt64 vfpRegisters[32];
    SceUInt32 SPSR, FPSCR, FPEXC;
    SceUInt32 FSR, FAR;
    SceUInt32 exceptionType;
};

typedef void (*KuKernelExceptionHandler)(KuKernelExceptionContext *);

struct KuKernelExceptionHandlerOpt {
    SceSize size;
};

// Kernel options for kuKernelAllocMemBlock
struct SceKernelAddrPair {
    uint32_t addr;
    uint32_t length;
};

struct SceKernelPaddrList {
    uint32_t size;
    uint32_t list_size;
    uint32_t ret_length;
    uint32_t ret_count;
    Ptr<SceKernelAddrPair> list;
};

struct SceKernelAllocMemBlockKernelOpt {
    SceSize    size;
    SceUInt32  field_4;
    SceUInt32  attr;
    SceUInt32  field_C;
    SceUInt32  paddr;
    SceSize    alignment;
    SceUInt32  extraLow;
    SceUInt32  extraHigh;
    SceUInt32  mirror_blockid;
    SceUID     pid;
    Ptr<SceKernelPaddrList> paddr_list;
    SceUInt32  field_2C[9];
};

// Options for kuKernelMemCommit
struct KuKernelMemCommitOpt {
    SceSize   size;
    SceUInt32 attr;
    SceUID    baseBlock;
    SceUInt32 baseOffset;
};

// Deprecated abort handler types
#define KU_KERNEL_ABORT_TYPE_DATA_ABORT     0
#define KU_KERNEL_ABORT_TYPE_PREFETCH_ABORT 1

struct KuKernelAbortContext {
    SceUInt32 r0, r1, r2, r3, r4, r5, r6, r7;
    SceUInt32 r8, r9, r10, r11, r12;
    SceUInt32 sp, lr, pc;
    SceUInt64 vfpRegisters[32];
    SceUInt32 SPSR, FPSCR, FPEXC;
    SceUInt32 FSR, FAR;
    SceUInt32 abortType;
};

typedef void (*KuKernelAbortHandler)(KuKernelAbortContext *);

struct KuKernelAbortHandlerOpt {
    SceSize size;
};

// ------------------------------------------------------------------
// kuKernelAllocMemBlock
// Allocate a memory block using kernel-level options (bypass user restrictions).
DECL_EXPORT(SceUID, kuKernelAllocMemBlock, const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockKernelOpt *opt);

// ------------------------------------------------------------------
// kuKernelFlushCaches
// Flush instruction and data caches for the given address range.
DECL_EXPORT(void, kuKernelFlushCaches, Ptr<const void> ptr, SceSize len);

// ------------------------------------------------------------------
// kuKernelCpuUnrestrictedMemcpy
// Copy memory without address-space restriction checks.
DECL_EXPORT(int, kuKernelCpuUnrestrictedMemcpy, Ptr<void> dst, Ptr<const void> src, SceSize len);

// ------------------------------------------------------------------
// kuPowerGetSysClockFrequency
// Get current system clock frequency (MHz).
DECL_EXPORT(int, kuPowerGetSysClockFrequency);

// ------------------------------------------------------------------
// kuPowerSetSysClockFrequency
// Set system clock frequency (MHz).
DECL_EXPORT(int, kuPowerSetSysClockFrequency, int freq);

// ------------------------------------------------------------------
// kuKernelMemProtect
// Change memory protection for an address range.
DECL_EXPORT(int, kuKernelMemProtect, Ptr<void> addr, SceSize len, SceUInt32 prot);

// ------------------------------------------------------------------
// kuKernelMemReserve
// Reserve a virtual memory region without committing physical pages.
DECL_EXPORT(SceUID, kuKernelMemReserve, Ptr<Ptr<void>> addr, SceSize size, SceKernelMemBlockType memBlockType);

// ------------------------------------------------------------------
// kuKernelMemCommit
// Commit physical memory pages within a previously reserved region.
DECL_EXPORT(int, kuKernelMemCommit, Ptr<void> addr, SceSize len, SceUInt32 prot, KuKernelMemCommitOpt *pOpt);

// ------------------------------------------------------------------
// kuKernelMemDecommit
// Decommit physical pages within a reserved region.
DECL_EXPORT(int, kuKernelMemDecommit, Ptr<void> addr, SceSize len);

// ------------------------------------------------------------------
// kuKernelRegisterExceptionHandler
// Register an exception handler for a given exception type.
DECL_EXPORT(int, kuKernelRegisterExceptionHandler, SceUInt32 exceptionType, Ptr<KuKernelExceptionHandler> pHandler, Ptr<KuKernelExceptionHandler> pOldHandler, KuKernelExceptionHandlerOpt *pOpt);

// ------------------------------------------------------------------
// kuKernelReleaseExceptionHandler
// Release a previously registered exception handler.
DECL_EXPORT(void, kuKernelReleaseExceptionHandler, SceUInt32 exceptionType);

// ------------------------------------------------------------------
// kuKernelRegisterAbortHandler (Deprecated)
// Register an abort handler.
DECL_EXPORT(int, kuKernelRegisterAbortHandler, Ptr<KuKernelAbortHandler> pHandler, Ptr<KuKernelAbortHandler> pOldHandler, KuKernelAbortHandlerOpt *pOpt);

// ------------------------------------------------------------------
// kuKernelReleaseAbortHandler (Deprecated)
// Release a previously registered abort handler.
DECL_EXPORT(void, kuKernelReleaseAbortHandler);
