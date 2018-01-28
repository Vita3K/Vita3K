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

#include <SceKernelModulemgr/exports.h>

EXPORT(int, sceKernelGetAllowedSdkVersionOnSystem) {
    return unimplemented("sceKernelGetAllowedSdkVersionOnSystem");
}

EXPORT(int, sceKernelGetLibraryInfoByNID) {
    return unimplemented("sceKernelGetLibraryInfoByNID");
}

EXPORT(int, sceKernelGetModuleIdByAddr) {
    return unimplemented("sceKernelGetModuleIdByAddr");
}

EXPORT(int, sceKernelGetModuleInfo) {
    return unimplemented("sceKernelGetModuleInfo");
}

EXPORT(int, sceKernelGetModuleList) {
    return unimplemented("sceKernelGetModuleList");
}

EXPORT(int, sceKernelGetSystemSwVersion) {
    return unimplemented("sceKernelGetSystemSwVersion");
}

EXPORT(int, sceKernelIsCalledFromSysModule) {
    return unimplemented("sceKernelIsCalledFromSysModule");
}

BRIDGE_IMPL(sceKernelGetAllowedSdkVersionOnSystem)
BRIDGE_IMPL(sceKernelGetLibraryInfoByNID)
BRIDGE_IMPL(sceKernelGetModuleIdByAddr)
BRIDGE_IMPL(sceKernelGetModuleInfo)
BRIDGE_IMPL(sceKernelGetModuleList)
BRIDGE_IMPL(sceKernelGetSystemSwVersion)
BRIDGE_IMPL(sceKernelIsCalledFromSysModule)
