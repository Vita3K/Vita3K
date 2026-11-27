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

// Writes the status back after a module syscall, which fails the call when it isn't mapped
static int write_status(EmuEnvState &emuenv, const char *export_name, Ptr<SceInt32> out, SceInt32 status, int result) {
    if (!out)
        return result;
    if (!is_valid_addr_len(emuenv.mem, out.address(), sizeof(SceInt32)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    *out.get(emuenv.mem) = status;
    return result;
}

// Tells if a module syscall's option is set but the bytes the syscall copies of it aren't mapped
static bool is_unreadable(const MemState &mem, Ptr<const void> option, SceSize size) {
    return option && !is_valid_addr_len(mem, option.address(), size);
}

// The module syscalls copy their block in first, then the option. They take only load flag 0x10
// and no start or stop flags, and a start or stop needs the status pointer. The module id is looked
// up before those checks, and the status is written back after the call, 0 after an error.
EXPORT(SceUID, _sceKernelLoadStartModule, const char *path, SceSize args, Ptr<const void> argp, Ptr<const SceKernelModuleCall_opt> opt) {
    TRACY_FUNC(_sceKernelLoadStartModule, path, args, argp, opt);
    if (!is_valid_addr_len(emuenv.mem, opt.address(), sizeof(SceKernelModuleCall_opt)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    const SceKernelModuleCall_opt block = *opt.get(emuenv.mem);
    if (is_unreadable(emuenv.mem, block.option, sizeof(SceSize)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    SceInt32 status = 0;
    SceUID result;
    if (block.flags & ~0x10)
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_FLAGS);
    else
        result = kernel_load_start_module(emuenv, export_name, path, args, argp, &status);
    return write_status(emuenv, export_name, block.status, status, result);
}

EXPORT(int, _sceKernelOpenModule) {
    TRACY_FUNC(_sceKernelOpenModule);
    return UNIMPLEMENTED();
}

EXPORT(int, _sceKernelStartModule, SceUID uid, SceSize args, Ptr<const void> argp, Ptr<const SceKernelModuleCall_opt> opt) {
    TRACY_FUNC(_sceKernelStartModule, uid, args, argp, opt);
    if (!is_valid_addr_len(emuenv.mem, opt.address(), sizeof(SceKernelModuleCall_opt)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    const SceKernelModuleCall_opt block = *opt.get(emuenv.mem);
    if (is_unreadable(emuenv.mem, block.option, sizeof(SceKernelStartModuleOpt)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    SceInt32 status = 0;
    int result;
    if (!lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex))
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_UID);
    else if (block.flags)
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_FLAGS);
    else if (!block.status)
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    else
        result = kernel_start_module(emuenv, export_name, uid, args, argp, &status);
    return write_status(emuenv, export_name, block.status, status, result);
}

EXPORT(int, _sceKernelStopModule, SceUID uid, SceSize args, Ptr<const void> argp, Ptr<const SceKernelModuleCall_opt> opt) {
    TRACY_FUNC(_sceKernelStopModule, uid, args, argp, opt);
    if (!is_valid_addr_len(emuenv.mem, opt.address(), sizeof(SceKernelModuleCall_opt)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    const SceKernelModuleCall_opt block = *opt.get(emuenv.mem);
    if (is_unreadable(emuenv.mem, block.option, sizeof(SceKernelStopModuleOpt)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    SceInt32 status = 0;
    int result;
    if (!lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex))
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_UID);
    else if (block.flags)
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_FLAGS);
    else if (!block.status)
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    else
        result = kernel_stop_module(emuenv, export_name, uid, args, argp, &status);
    return write_status(emuenv, export_name, block.status, status, result);
}

EXPORT(int, _sceKernelStopUnloadModule, SceUID uid, SceSize args, Ptr<const void> argp, Ptr<const SceKernelModuleCall_opt> opt) {
    TRACY_FUNC(_sceKernelStopUnloadModule, uid, args, argp, opt);
    if (!is_valid_addr_len(emuenv.mem, opt.address(), sizeof(SceKernelModuleCall_opt)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    const SceKernelModuleCall_opt block = *opt.get(emuenv.mem);
    if (is_unreadable(emuenv.mem, block.option, sizeof(SceSize)))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS);
    SceInt32 status = 0;
    int result;
    if (!lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex))
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_UID);
    else if (block.flags)
        result = RET_ERROR(SCE_KERNEL_ERROR_INVALID_FLAGS);
    else
        result = kernel_stop_unload_module(emuenv, export_name, uid, args, argp, &status);
    return write_status(emuenv, export_name, block.status, status, result);
}

EXPORT(int, _sceKernelUnloadModule, SceUID uid, SceUInt32 flags, const void *pOpt) {
    TRACY_FUNC(_sceKernelUnloadModule);
    if (!lock_and_find(uid, emuenv.kernel.loaded_modules, emuenv.kernel.mutex))
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_UID);
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
        return RET_ERROR(SCE_KERNEL_ERROR_INVALID_UID);
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
