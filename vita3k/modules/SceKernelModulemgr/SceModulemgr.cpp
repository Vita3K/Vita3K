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

#include "SceModulemgr.h"
#include <io/functions.h>
#include <kernel/load_self.h>
#include <kernel/state.h>

#include <modules/module_parent.h>
#include <util/lock_and_find.h>

/**
 * \brief Loads a dynamic module into memory if it wasn't already loaded. If it was, find it and return it. First 3 arguments are outputs.
 * \param mod_id UID of the loaded module object
 * \param entry_point Entry point (module_start) of the loaded module
 * \param module Module info
 * \param emuenv PlayStation Vita emulated environment
 * \param export_name
 * \param path File name of module file
 * \param error_val Error value on failure
 * \return True on success, false on failure
 */
static bool load_module(SceUID &mod_id, Ptr<const void> &entry_point, SceKernelModuleInfoPtr &module, EmuEnvState &emuenv, const char *export_name, const char *path, int &error_val) {
    const auto &loaded_modules = emuenv.kernel.loaded_modules;

    auto module_iter = std::find_if(loaded_modules.begin(), loaded_modules.end(), [path](const auto &p) {
        return std::string(p.second->path) == path;
    });

    if (module_iter == loaded_modules.end()) {
        // module is not loaded, load it here

        const auto file = open_file(emuenv.io, path, SCE_O_RDONLY, emuenv.pref_path, export_name);
        if (file < 0) {
            error_val = RET_ERROR(file);
            return false;
        }
        const auto size = seek_file(file, 0, SCE_SEEK_END, emuenv.io, export_name);
        if (size < 0) {
            error_val = RET_ERROR(SCE_ERROR_ERRNO_EINVAL);
            return false;
        }

        if (seek_file(file, 0, SCE_SEEK_SET, emuenv.io, export_name) < 0) {
            error_val = RET_ERROR(static_cast<int>(size));
            return false;
        }

        std::vector<char> data(static_cast<int>(size) + 1); // null-terminated char array
        if (read_file(data.data(), emuenv.io, file, SceSize(size), export_name) < 0) {
            data.clear();
            error_val = RET_ERROR(static_cast<int>(size));
            return false;
        }

        mod_id = load_self(entry_point, emuenv.kernel, emuenv.mem, data.data(), path);

        close_file(emuenv.io, file, export_name);
        data.clear();
        if (mod_id < 0) {
            error_val = RET_ERROR(mod_id);
            return false;
        }

        module_iter = loaded_modules.find(mod_id);
        module = module_iter->second;
    } else {
        // module is already loaded
        module = module_iter->second;

        mod_id = module_iter->first;
        entry_point = module->start_entry;
    }
    return true;
}

EXPORT(int, _sceKernelCloseModule) {
    return UNIMPLEMENTED();
}

EXPORT(SceUID, _sceKernelLoadModule, char *path, int flags, SceKernelLMOption *option) {
    SceUID mod_id;
    Ptr<const void> entry_point;
    SceKernelModuleInfoPtr module;

    int error_val;
    if (!load_module(mod_id, entry_point, module, emuenv, export_name, path, error_val))
        return error_val;

    return mod_id;
}

static SceUID start_module(EmuEnvState &emuenv, const SceKernelModuleInfoPtr &module, SceSize args, const Ptr<void> argp, int *pRes) {
    auto module_thread = emuenv.kernel.create_thread(emuenv.mem, module->module_name);
    uint32_t result = module_thread->run_guest_function(module->start_entry.address(), { args, argp.address() });
    emuenv.kernel.exit_delete_thread(module_thread);

    LOG_INFO("Module {} (at \"{}\") module_start returned {}", module->module_name, module->path, log_hex(result));

    if (pRes)
        *pRes = result;

    return module->modid;
}

EXPORT(SceUID, _sceKernelLoadStartModule, const char *moduleFileName, SceSize args, const Ptr<void> argp, SceUInt32 flags, const SceKernelLMOption *pOpt, int *pRes) {
    // Is workaround for fix crash on loading "rgpluginsgm_psvita" module, relate issue #1095 on github, delete this after fix it.
    if (std::string(moduleFileName).find("rgpluginsgm_psvita") != std::string::npos) {
        LOG_WARN("Bypass load this module: {}", moduleFileName);
        return SCE_KERNEL_ERROR_MODULEMGR_INVALID_TYPE;
    }

    SceUID mod_id;
    Ptr<const void> entry_point;
    SceKernelModuleInfoPtr module;

    int error_val;
    if (!load_module(mod_id, entry_point, module, emuenv, export_name, moduleFileName, error_val))
        return error_val;

    return start_module(emuenv, module, args, argp, pRes);
}

EXPORT(int, _sceKernelOpenModule) {
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelStartModule, SceUID uid, SceSize args, const Ptr<void> argp, SceUInt32 flags, const Ptr<SceKernelStartModuleOpt> pOpt, int *pRes) {
    const SceKernelModuleInfoPtr module = lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex);

    return start_module(emuenv, module, args, argp, pRes);
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

EXPORT(int, sceKernelGetModuleIdByAddr, Ptr<void> addr) {
    KernelState *const state = &emuenv.kernel;

    for (const auto &module : state->loaded_modules) {
        for (int n = 0; n < MODULE_INFO_NUM_SEGMENTS; n++) {
            const auto segment_address_begin = module.second->segments[n].vaddr.address();
            const auto segment_address_end = segment_address_begin + module.second->segments[n].memsz;
            if (addr.address() > segment_address_begin && addr.address() < segment_address_end) {
                return module.first;
            }
        }
    }

    return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NOENT);
}

EXPORT(int, sceKernelGetModuleInfo, SceUID modid, SceKernelModuleInfo *info) {
    KernelState *const state = &emuenv.kernel;
    const SceKernelModuleInfoPtrs::const_iterator module = state->loaded_modules.find(modid);
    assert(module != state->loaded_modules.end());

    memcpy(info, module->second.get(), module->second.get()->size);

    return SCE_KERNEL_OK;
}

EXPORT(int, sceKernelGetModuleList, int flags, SceUID *modids, int *num) {
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);
    int i = 0;
    for (SceKernelModuleInfoPtrs::iterator module = emuenv.kernel.loaded_modules.begin(); module != emuenv.kernel.loaded_modules.end(); ++module) {
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
