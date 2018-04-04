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

#include "SceModulemgr.h"
#include "kernel/state.h"
#include <psp2/kernel/error.h>

EXPORT(int, sceKernelGetAllowedSdkVersionOnSystem) {
    return unimplemented("sceKernelGetAllowedSdkVersionOnSystem");
}

EXPORT(int, sceKernelGetLibraryInfoByNID) {
    return unimplemented("sceKernelGetLibraryInfoByNID");
}

EXPORT(int, sceKernelGetModuleIdByAddr) {
    return unimplemented("sceKernelGetModuleIdByAddr");
}

EXPORT(int, sceKernelGetModuleInfo, SceUID modid, SceKernelModuleInfo *info) {
    KernelState *const state = &host.kernel;
    const SceKernelModuleInfoPtrs::const_iterator module = state->loaded_modules.find(modid);
    assert(module != state->loaded_modules.end());

    memcpy(info, module->second.get(), module->second.get()->size);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetModuleList, int flags, SceUID *modids, int *num) {
    const std::unique_lock<std::mutex> lock(host.kernel.mutex);
    int i = 0;
    for (SceKernelModuleInfoPtrs::iterator module = host.kernel.loaded_modules.begin(); module != host.kernel.loaded_modules.end(); ++module) {
        modids[i] = module->first;
        i++;
    }
    *num = i;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetSystemSwVersion) {
    return unimplemented("sceKernelGetSystemSwVersion");
}

EXPORT(int, sceKernelInhibitLoadingModule) {
    return unimplemented("sceKernelInhibitLoadingModule");
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
BRIDGE_IMPL(sceKernelInhibitLoadingModule)
BRIDGE_IMPL(sceKernelIsCalledFromSysModule)
