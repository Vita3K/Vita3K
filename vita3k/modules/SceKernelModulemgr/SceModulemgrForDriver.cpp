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

#include "../SceLibKernel/SceLibKernel.h"
#include "SceModulemgr.h"
#include <kernel/state.h>
#include <module/module.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceModulemgrForDriver);

struct SceKernelULMOption;

EXPORT(int, ksceKernelGetModuleInfoByAddr, SceUID pid, Ptr<void> module_addr, SceKernelModuleInfo *info) {
    TRACY_FUNC(ksceKernelGetModuleInfoByAddr, pid, module_addr, info);
    return CALL_EXPORT(sceKernelGetModuleInfoByAddr, module_addr, info);
}

EXPORT(SceUID, ksceKernelLoadModule, char *path, int flags, SceKernelLMOption *option) {
    TRACY_FUNC(ksceKernelLoadModule, path, flags, option);
    return CALL_EXPORT(_sceKernelLoadModule, path, flags, option);
}

EXPORT(SceUID, ksceKernelLoadStartModule, const char *path, SceSize args, Ptr<const void> argp, int flags, SceKernelLMOption *option, int *status) {
    TRACY_FUNC(ksceKernelLoadStartModule, path, args, argp, flags, option, status);
    return CALL_EXPORT(_sceKernelLoadStartModule, path, args, argp, flags, option, status);
}

EXPORT(SceUID, ksceKernelLoadStartModuleForPid, SceUID pid, const char *path, SceSize args, Ptr<const void> argp, int flags, SceKernelLMOption *option, int *status) {
    TRACY_FUNC(ksceKernelLoadStartModuleForPid, pid, path, args, argp, flags, option, status);
    return CALL_EXPORT(ksceKernelLoadStartModule, path, args, argp, flags, option, status);
}

EXPORT(SceUID, ksceKernelLoadStartSharedModuleForPid, SceUID pid, const char *path, SceSize args, Ptr<const void> argp, int flags, SceKernelLMOption *option, int *status) {
    TRACY_FUNC(ksceKernelLoadStartSharedModuleForPid, pid, path, args, argp, flags, option, status);
    return CALL_EXPORT(ksceKernelLoadStartModuleForPid, pid, path, args, argp, flags, option, status);
}

EXPORT(int, ksceKernelRegisterLibary, const char *library_name, int version, void *entry_table) {
    TRACY_FUNC(ksceKernelRegisterLibary, library_name, version, entry_table);
    return UNIMPLEMENTED();
}

EXPORT(int, ksceKernelReleaseLibary, const char *library_name) {
    TRACY_FUNC(ksceKernelReleaseLibary, library_name);
    return UNIMPLEMENTED();
}

EXPORT(SceUID, ksceKernelSearchModuleByName, const char *module_name) {
    TRACY_FUNC(ksceKernelSearchModuleByName, module_name);
    const std::lock_guard<std::mutex> lock(emuenv.kernel.mutex);

    for (const auto &[module_id, module] : emuenv.kernel.loaded_modules) {
        if (std::strncmp(module->info.module_name, module_name, sizeof(module->info.module_name)) == 0)
            return module_id;
    }

    return RET_ERROR(SCE_KERNEL_ERROR_MODULEMGR_NO_MOD);
}

EXPORT(int, ksceKernelStartModule, SceUID modid, SceSize args, Ptr<const void> argp, int flags, SceKernelLMOption *option, int *status) {
    TRACY_FUNC(ksceKernelStartModule, modid, args, argp, flags, option, status);
    return CALL_EXPORT(_sceKernelStartModule, modid, args, argp, flags, reinterpret_cast<const SceKernelStartModuleOpt *>(option), status);
}

EXPORT(int, ksceKernelStopModule, SceUID modid, SceSize args, Ptr<const void> argp, int flags, SceKernelULMOption *option, int *status) {
    TRACY_FUNC(ksceKernelStopModule, modid, args, argp, flags, option, status);
    return CALL_EXPORT(_sceKernelStopModule, modid, args, argp, flags, reinterpret_cast<const SceKernelStopModuleOpt *>(option), status);
}

EXPORT(int, ksceKernelStopUnloadModule, SceUID modid, SceSize args, Ptr<const void> argp, int flags, SceKernelULMOption *option, int *status) {
    TRACY_FUNC(ksceKernelStopUnloadModule, modid, args, argp, flags, option, status);
    return CALL_EXPORT(_sceKernelStopUnloadModule, modid, args, argp, flags, option, status);
}

EXPORT(int, ksceKernelStopUnloadModuleForPid, SceUID pid, SceUID modid, SceSize args, Ptr<const void> argp, int flags, SceKernelULMOption *option, int *status) {
    TRACY_FUNC(ksceKernelStopUnloadModuleForPid, pid, modid, args, argp, flags, option, status);
    return CALL_EXPORT(ksceKernelStopUnloadModule, modid, args, argp, flags, option, status);
}

EXPORT(int, ksceKernelStopUnloadSharedModuleForPid, SceUID pid, SceUID modid, SceSize args, Ptr<const void> argp, int flags, SceKernelULMOption *option, int *status) {
    TRACY_FUNC(ksceKernelStopUnloadSharedModuleForPid, pid, modid, args, argp, flags, option, status);
    return CALL_EXPORT(ksceKernelStopUnloadModuleForPid, pid, modid, args, argp, flags, option, status);
}

EXPORT(int, ksceKernelUnloadModule, SceUID modid, int flags, SceKernelULMOption *option) {
    TRACY_FUNC(ksceKernelUnloadModule, modid, flags, option);
    return CALL_EXPORT(_sceKernelUnloadModule, modid, flags, option);
}
