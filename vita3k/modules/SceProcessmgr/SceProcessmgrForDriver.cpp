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

#include <module/module.h>

#include <atomic>

static std::atomic<int> pls_key_counter{ 1 };

EXPORT(int, ksceKernelCreateProcessLocalStorage, const char *name, SceSize size) {
    // Return a unique key for this PLS entry.
    // kubridge uses this for per-process exception handler context.
    return pls_key_counter.fetch_add(1);
}

EXPORT(int, ksceKernelGetProcessInfo) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetProcessLocalStorageAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelGetProcessLocalStorageAddrForPid) {
    return UNIMPLEMENTED();
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
    STUBBED("ksceKernelAllocRemoteProcessHeap");
    return 0;
}

EXPORT(int, ksceKernelFreeRemoteProcessHeap, SceUID pid, Ptr<void> ptr) {
    STUBBED("ksceKernelFreeRemoteProcessHeap");
    return 0;
}
