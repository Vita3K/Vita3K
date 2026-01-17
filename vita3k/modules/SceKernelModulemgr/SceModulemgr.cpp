// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <io/functions.h>
#include <kernel/load_self.h>
#include <kernel/state.h>

#include <modules/module_parent.h>
#include <util/lock_and_find.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceModulemgr);

EXPORT(int, _sceKernelCloseModule) {
    TRACY_FUNC(_sceKernelCloseModule);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, _sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option) {
    TRACY_FUNC(_sceKernelLoadModule, path, flags, option);
    return load_module(emuenv, path);
}

static SceUID kernel_start_module(EmuEnvState &emuenv, SceUID module_id, SceSize args, Ptr<const void> argp, int *pRes) {
    const SceKernelModulePtr module = lock_and_find(module_id, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        const char *export_name = __FUNCTION__;
        return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NO_MOD);
    }
    auto result = start_module(emuenv, module->info, args, argp);
    if (pRes)
        *pRes = result;

    return module->info.modid;
}

static int kernel_stop_module(EmuEnvState &emuenv, SceUID module_id, SceSize args, Ptr<const void> argp, int *pRes) {
    const SceKernelModulePtr module = lock_and_find(module_id, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);
    if (!module) {
        const char *export_name = __FUNCTION__;
        return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NO_MOD);
    }
    auto result = stop_module(emuenv, module->info, args, argp);
    if (pRes)
        *pRes = result;
    return 0;
}

EXPORT(SceUID, _sceKernelLoadStartModule, const char *moduleFileName, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelLoadStartModule, moduleFileName, args, argp, flags, pOpt, pRes);
    // Is workaround for fix crash on loading "rgpluginsgm_psvita" module, relate issue #1095 on github, delete this after fix it.
    if (std::string_view(moduleFileName).find("rgpluginsgm_psvita") != std::string::npos) {
        LOG_WARN("Bypass load this module: {}", moduleFileName);
        return SCE_KERNEL_ERROR_MODULEMGR_INVALID_TYPE;
    }

    SceUID module_id = load_module(emuenv, moduleFileName);
    if (module_id < 0)
        return module_id;
    return kernel_start_module(emuenv, module_id, args, argp, pRes);
}

EXPORT(SceUID, _sceKernelOpenModule, const char *moduleFileName, SceSize args, const Ptr<void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelOpenModule, moduleFileName, args, argp, flags, pOpt, pRes);

    return CALL_EXPORT(_sceKernelLoadStartModule, moduleFileName, args, argp, flags, pOpt, pRes);
}

EXPORT(int, _sceKernelStartModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelStartModuleOpt *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelStartModule, uid, args, argp, flags, pOpt, pRes);

    return kernel_start_module(emuenv, uid, args, argp, pRes);
}

EXPORT(int, _sceKernelStopModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelStopModuleOpt *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelStopModule, uid, args, argp, flags, pOpt, pRes);
    return kernel_stop_module(emuenv, uid, args, argp, pRes);
}

EXPORT(int, _sceKernelStopUnloadModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const void *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelStopUnloadModule, uid, args, argp, flags, pOpt, pRes);
    int ret = kernel_stop_module(emuenv, uid, args, argp, pRes);
    if (ret < 0)
        return ret;

    return unload_module(emuenv, uid);
}

EXPORT(int, _sceKernelUnloadModule, SceUID uid, SceUInt32 flags, const void *pOpt) {
    TRACY_FUNC(_sceKernelUnloadModule);
    return unload_module(emuenv, uid);
}

EXPORT(int, sceKernelGetAllowedSdkVersionOnSystem) {
    TRACY_FUNC(sceKernelGetAllowedSdkVersionOnSystem);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetLibraryInfoByNID) {
    TRACY_FUNC(sceKernelGetLibraryInfoByNID);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelGetModuleIdByAddr, Ptr<void> addr) {
    TRACY_FUNC(sceKernelGetModuleIdByAddr, addr);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &module : emuenv.kernel.loaded_modules) {
        for (auto &segment : module.second->info.segments) {
            const auto segment_address_begin = segment.vaddr.address();
            const auto segment_address_end = segment_address_begin + segment.memsz;
            if (addr.address() > segment_address_begin && addr.address() < segment_address_end) {
                return module.first;
            }
        }
    }

    return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NOENT);
}

EXPORT(int, sceKernelGetModuleInfo, SceUID modid, SceKernelModuleInfo *info) {
    TRACY_FUNC(sceKernelGetModuleInfo, modid, info);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    auto module = emuenv.kernel.loaded_modules.find(modid);
    if (module == emuenv.kernel.loaded_modules.end()) {
        return RET_ERROR(SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD);
    }

    memcpy(info, &module->second->info, module->second->info.size);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetModuleList, int flags, SceUID *modids, int *num) {
    TRACY_FUNC(sceKernelGetModuleList, flags, modids, num);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    // for Maidump main module should be the last module
    int i = 0;
    SceUID main_module_id = 0;
    for (auto &[module_id, module] : emuenv.kernel.loaded_modules) {
        if (module->info.path == "app0:" + emuenv.self_path) {
            main_module_id = module_id;
        } else {
            modids[i] = module_id;
            i++;
        }
    }
    if (main_module_id != 0) {
        modids[i] = main_module_id;
        i++;
    }
    *num = i;
    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetSystemSwVersion) {
    TRACY_FUNC(sceKernelGetSystemSwVersion);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelInhibitLoadingModule) {
    TRACY_FUNC(sceKernelInhibitLoadingModule);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelIsCalledFromSysModule) {
    TRACY_FUNC(sceKernelIsCalledFromSysModule);
    return UNIMPLEMENTED();
}
