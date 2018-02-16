// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include "SceGpuEs4ForUser.h"

EXPORT(int, PVRSRVOpen) {
    return unimplemented("PVRSRVOpen");
}

EXPORT(int, PVRSRVRelease) {
    return unimplemented("PVRSRVRelease");
}

EXPORT(int, PVRSRV_BridgeDispatchKM) {
    return unimplemented("PVRSRV_BridgeDispatchKM");
}

EXPORT(int, sceGpuRegisterSalvage) {
    return unimplemented("sceGpuRegisterSalvage");
}

EXPORT(int, sceGpuSignalWait) {
    return unimplemented("sceGpuSignalWait");
}

EXPORT(int, sceGpuSignalWaitLockup) {
    return unimplemented("sceGpuSignalWaitLockup");
}

EXPORT(int, sceGpuUnregisterSalvage) {
    return unimplemented("sceGpuUnregisterSalvage");
}

BRIDGE_IMPL(PVRSRVOpen)
BRIDGE_IMPL(PVRSRVRelease)
BRIDGE_IMPL(PVRSRV_BridgeDispatchKM)
BRIDGE_IMPL(sceGpuRegisterSalvage)
BRIDGE_IMPL(sceGpuSignalWait)
BRIDGE_IMPL(sceGpuSignalWaitLockup)
BRIDGE_IMPL(sceGpuUnregisterSalvage)
