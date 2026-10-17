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

#include "SceModulemgr.h"
#include <cpu/state.h>
#include <io/functions.h>
#include <kernel/load_self.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>

#include <modules/module_parent.h>
#include <modules/system_version.h>
#include <util/lock_and_find.h>

#include <cstring>
#include <limits>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceModulemgr);

EXPORT(int, _sceKernelCloseModule) {
    TRACY_FUNC(_sceKernelCloseModule);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, _sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option) {
    TRACY_FUNC(_sceKernelLoadModule, path, flags, option);
    if (const SceInt32 error = emuenv.kernel.check_primary_module_loader(thread_id))
        return RET_ERROR(error);
    return load_module(emuenv, path);
}

static SceUID kernel_start_module(EmuEnvState &emuenv, SceUID module_id, SceSize args, Ptr<const void> argp, int *pRes) {
    const SceKernelModulePtr module = emuenv.kernel.find_module(emuenv.mem, module_id);
    if (!module) {
        const char *export_name = __FUNCTION__;
        return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NO_MOD);
    }
    auto result = start_module(emuenv, *module, args, argp);
    if (pRes)
        *pRes = result;

    return module->info.modid;
}

static int kernel_stop_module(EmuEnvState &emuenv, SceUID module_id, SceSize args, Ptr<const void> argp, int *pRes) {
    const SceKernelModulePtr module = emuenv.kernel.find_module(emuenv.mem, module_id);
    if (!module) {
        const char *export_name = __FUNCTION__;
        return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NO_MOD);
    }
    auto result = stop_module(emuenv, *module, args, argp);
    if (pRes)
        *pRes = result;
    return 0;
}

EXPORT(SceUID, _sceKernelLoadStartModule, const char *moduleFileName, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelLoadStartModule, moduleFileName, args, argp, flags, pOpt, pRes);
    if (!moduleFileName)
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    if (const SceInt32 error = emuenv.kernel.check_primary_module_loader(thread_id))
        return RET_ERROR(error);
    // Is workaround for fix crash on loading "rgpluginsgm_psvita" module, relate issue #1095 on github, delete this after fix it.
    if (std::string_view(moduleFileName).contains("rgpluginsgm_psvita")) {
        LOG_WARN("Bypass load this module: {}", moduleFileName);
        return SCE_KERNEL_ERROR_MODULEMGR_INVALID_TYPE;
    }

    SceUID module_id = load_module(emuenv, moduleFileName);
    if (module_id < 0)
        return module_id;
    return kernel_start_module(emuenv, module_id, args, argp, pRes);
}

EXPORT(SceUID, _sceKernelOpenModule, const char *moduleFileName, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelOpenModule, moduleFileName, args, argp, flags, pOpt, pRes);
    return CALL_EXPORT(_sceKernelLoadStartModule, moduleFileName, args, argp, flags, pOpt, pRes);
}

EXPORT(int, _sceKernelStartModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelStartModuleOpt *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelStartModule, uid, args, argp, flags, pOpt, pRes);
    if (const SceInt32 error = emuenv.kernel.check_primary_module_loader(thread_id))
        return RET_ERROR(error);
    return kernel_start_module(emuenv, uid, args, argp, pRes);
}

EXPORT(int, _sceKernelStopModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const SceKernelStopModuleOpt *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelStopModule, uid, args, argp, flags, pOpt, pRes);
    if (const SceInt32 error = emuenv.kernel.check_primary_module_loader(thread_id))
        return RET_ERROR(error);
    return kernel_stop_module(emuenv, uid, args, argp, pRes);
}

EXPORT(int, _sceKernelStopUnloadModule, SceUID uid, SceSize args, Ptr<const void> argp, SceUInt32 flags, const void *pOpt, int *pRes) {
    TRACY_FUNC(_sceKernelStopUnloadModule, uid, args, argp, flags, pOpt, pRes);
    if (const SceInt32 error = emuenv.kernel.check_primary_module_loader(thread_id))
        return RET_ERROR(error);
    int ret = kernel_stop_module(emuenv, uid, args, argp, pRes);
    if (ret < 0)
        return ret;

    return unload_module(emuenv, uid);
}

EXPORT(int, _sceKernelUnloadModule, SceUID uid, SceUInt32 flags, const void *pOpt) {
    TRACY_FUNC(_sceKernelUnloadModule);
    if (const SceInt32 error = emuenv.kernel.check_primary_module_loader(thread_id))
        return RET_ERROR(error);
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
    const ThreadStatePtr caller = emuenv.kernel.get_thread(thread_id);
    if (!caller)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &module : emuenv.kernel.loaded_modules) {
        if (module.second->address_space != &caller->cpu->mem)
            continue;
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
    const ThreadStatePtr caller = emuenv.kernel.get_thread(thread_id);
    if (!caller)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    const auto module = emuenv.kernel.find_module(caller->cpu->mem, modid);
    if (!module) {
        return RET_ERROR(SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD);
    }

    memcpy(info, &module->info, module->info.size);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetModuleList, int flags, SceUID *modids, int *num) {
    TRACY_FUNC(sceKernelGetModuleList, flags, modids, num);
    const ThreadStatePtr caller = emuenv.kernel.get_thread(thread_id);
    if (!caller)
        return RET_ERROR(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    // for Maidump main module should be the last module
    int i = 0;
    SceUID main_module_id = 0;
    for (auto &[module_id, module] : emuenv.kernel.loaded_modules) {
        if (module->address_space != &caller->cpu->mem)
            continue;
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

EXPORT(int, sceKernelGetSystemSwVersion, Ptr<SceKernelSystemSwVersion> info) {
    TRACY_FUNC(sceKernelGetSystemSwVersion, info.address());
    if (!info || info.address() > std::numeric_limits<Address>::max() - (sizeof(SceKernelSystemSwVersion) - 1))
        return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    for (Address i = 0; i < sizeof(SceKernelSystemSwVersion); ++i) {
        if (!Ptr<uint8_t>(info.address() + i).valid(emuenv.mem))
            return RET_ERROR(SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    }
    SceKernelSystemSwVersion result{};
    auto *bytes = reinterpret_cast<uint8_t *>(&result);
    for (Address i = 0; i < sizeof(result.size); ++i)
        bytes[i] = *Ptr<const uint8_t>(info.address() + i).get(emuenv.mem);
    if (result.size != sizeof(result))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    // Vita3K exposes a compatibility version; it does not track the installed firmware version.
    LOG_WARN_ONCE("Reporting compatibility firmware version 3.74; installed firmware version is not tracked");
    std::memcpy(result.version_string, EMULATED_SYSTEM_VERSION_STRING, sizeof(EMULATED_SYSTEM_VERSION_STRING));
    result.version = EMULATED_SYSTEM_VERSION;
    for (Address i = 0; i < sizeof(result); ++i)
        *Ptr<uint8_t>(info.address() + i).get(emuenv.mem) = bytes[i];

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelInhibitLoadingModule) {
    TRACY_FUNC(sceKernelInhibitLoadingModule);
    return UNIMPLEMENTED();
}

EXPORT(int, sceKernelIsCalledFromSysModule) {
    TRACY_FUNC(sceKernelIsCalledFromSysModule);
    return UNIMPLEMENTED();
}
