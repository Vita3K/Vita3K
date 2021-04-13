// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include "SceProcessmgrForDriver.h"

EXPORT(int, ksceKernelCreateProcessLocalStorage) {
    return UNIMPLEMENTED();
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

BRIDGE_IMPL(ksceKernelCreateProcessLocalStorage)
BRIDGE_IMPL(ksceKernelGetProcessInfo)
BRIDGE_IMPL(ksceKernelGetProcessLocalStorageAddr)
BRIDGE_IMPL(ksceKernelGetProcessLocalStorageAddrForPid)
BRIDGE_IMPL(ksceKernelGetProcessStatus)
BRIDGE_IMPL(ksceKernelGetProcessTimeCore)
BRIDGE_IMPL(ksceKernelGetProcessTimeLowCore)
BRIDGE_IMPL(ksceKernelGetProcessTimeWideCore)
BRIDGE_IMPL(ksceKernelGetRemoteProcessTime)
BRIDGE_IMPL(ksceKernelIsCDialogAvailable)
BRIDGE_IMPL(ksceKernelIsGameBudget)
