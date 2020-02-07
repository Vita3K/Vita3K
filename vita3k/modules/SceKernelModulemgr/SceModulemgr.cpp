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
#include <kernel/state.h>

EXPORT(int, _sceKernelCloseModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLoadModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelLoadStartModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelOpenModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelStartModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelStopModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelStopUnloadModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelUnloadModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetAllowedSdkVersionOnSystem) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLibraryInfoByNID) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetModuleIdByAddr) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetModuleInfo, SceUID modid, SceKernelModuleInfo *info) {
    KernelState *const state = &host.kernel;
    const SceKernelModuleInfoPtrs::const_iterator module = state->loaded_modules.find(modid);
    assert(module != state->loaded_modules.end());

    memcpy(info, module->second.get(), module->second.get()->size);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetModuleList, int flags, SceUID *modids, int *num) {
    const std::lock_guard<std::mutex> lock(host.kernel.mutex);
    int i = 0;
    for (SceKernelModuleInfoPtrs::iterator module = host.kernel.loaded_modules.begin(); module != host.kernel.loaded_modules.end(); ++module) {
        modids[i] = module->first;
        i++;
    }
    *num = i;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetSystemSwVersion) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelInhibitLoadingModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelIsCalledFromSysModule) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(_sceKernelCloseModule)
BRIDGE_IMPL(_sceKernelLoadModule)
BRIDGE_IMPL(_sceKernelLoadStartModule)
BRIDGE_IMPL(_sceKernelOpenModule)
BRIDGE_IMPL(_sceKernelStartModule)
BRIDGE_IMPL(_sceKernelStopModule)
BRIDGE_IMPL(_sceKernelStopUnloadModule)
BRIDGE_IMPL(_sceKernelUnloadModule)
BRIDGE_IMPL(sceKernelGetAllowedSdkVersionOnSystem)
BRIDGE_IMPL(sceKernelGetLibraryInfoByNID)
BRIDGE_IMPL(sceKernelGetModuleIdByAddr)
BRIDGE_IMPL(sceKernelGetModuleInfo)
BRIDGE_IMPL(sceKernelGetModuleList)
BRIDGE_IMPL(sceKernelGetSystemSwVersion)
BRIDGE_IMPL(sceKernelInhibitLoadingModule)
BRIDGE_IMPL(sceKernelIsCalledFromSysModule)
