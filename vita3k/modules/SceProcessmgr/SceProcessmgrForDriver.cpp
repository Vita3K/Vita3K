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

#include "kernel/state.h"

#include <module/module.h>
#include <util/tracy.h>

TRACY_MODULE_NAME(SceProcessmgrForDriver)

EXPORT(int, ksceKernelCreateProcessLocalStorage, const char *name, SceSize size) {
    TRACY_FUNC(ksceKernelCreateProcessLocalStorage, name, size);
    // >> 1 to make result positive (not error).
    // result of memory allocation is always aligned (I think), so lowest bite is always 0
    // kubridge uses this for per-process exception handler context.
    auto pls_data = alloc(emuenv.mem, size, name) >> 1;
    return pls_data;
}

EXPORT(int, ksceKernelGetProcessInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetProcessLocalStorageAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetProcessLocalStorageAddrForPid, SceUID pid, int key, Ptr<void> *out_addr, int create_if_doesnt_exist) {
    TRACY_FUNC(ksceKernelGetProcessLocalStorageAddrForPid, pid, key, out_addr, create_if_doesnt_exist);
    // See ksceKernelCreateProcessLocalStorage
    *out_addr = key << 1;
    return 0;
}

EXPORT(int, ksceKernelGetProcessStatus) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetProcessTimeCore) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetProcessTimeLowCore) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetProcessTimeWideCore) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetRemoteProcessTime) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelIsCDialogAvailable) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelIsGameBudget) {
    return UNIMPLEMENTED();
}

// kubridge-required stubs
EXPORT(SceUID, ksceKernelAllocRemoteProcessHeap, SceUID pid, uint32_t size, Ptr<void> opt) {
    TRACY_FUNC(ksceKernelAllocRemoteProcessHeap, pid, size, opt);
    STUBBED("ksceKernelAllocRemoteProcessHeap");
    return emuenv.kernel.get_next_uid();
}

EXPORT(int, ksceKernelFreeRemoteProcessHeap, SceUID pid, Ptr<void> ptr) {
    TRACY_FUNC(ksceKernelFreeRemoteProcessHeap, pid, ptr);
    STUBBED("ksceKernelFreeRemoteProcessHeap");
    return 0;
}
