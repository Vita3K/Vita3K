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

#pragma once

#include <module/module.h>
#include <kernel/types.h>

// taiHEN hook/inject result type
typedef SceUID tai_hook_ref_t;

// taiHEN module info structure (as exposed to guest code).
// Matches the tai_module_info_t from yifanlu/taiHEN SDK.
struct tai_module_info_t {
    SceSize size;           ///< struct size, caller must set to sizeof(tai_module_info_t)
    SceUID modid;           ///< module UID
    char name[28];          ///< module name (null-terminated)
    uint32_t flags;         ///< module flags (from SCE elf)
    uint32_t exports_start; ///< start of module exports in memory
    uint32_t exports_end;   ///< end of module exports in memory
    uint32_t imports_start; ///< start of module imports in memory
    uint32_t imports_end;   ///< end of module imports in memory
};

DECL_EXPORT(SceUID, taiHookFunctionExport, Ptr<uint32_t> p_hook, const char *module_name, uint32_t library_nid, uint32_t func_nid, Ptr<const void> hook_func);
DECL_EXPORT(SceUID, taiHookFunctionImport, Ptr<uint32_t> p_hook, const char *module_name, uint32_t library_nid, uint32_t func_nid, Ptr<const void> hook_func);
DECL_EXPORT(SceUID, taiHookFunctionOffset, Ptr<uint32_t> p_hook, SceUID modid, int segidx, uint32_t offset, int thumb, Ptr<const void> hook_func);
DECL_EXPORT(SceUID, taiHookFunctionAbs, Ptr<uint32_t> p_hook, SceUID modid, Ptr<const void> func_addr, Ptr<const void> hook_func);
DECL_EXPORT(int, taiHookRelease, SceUID hook_uid, uint32_t hook);
DECL_EXPORT(SceUID, taiInjectData, SceUID modid, int segidx, uint32_t offset, Ptr<const void> data, SceSize size);
DECL_EXPORT(int, taiInjectRelease, SceUID inject_uid);
DECL_EXPORT(SceUID, taiLoadStartModule, const char *path, SceSize args, Ptr<const void> argp, int flags, Ptr<const void> opt, Ptr<int> res);
DECL_EXPORT(int, taiStopUnloadModule, SceUID uid, SceSize args, Ptr<const void> argp, int flags, Ptr<const void> opt, Ptr<int> res);
DECL_EXPORT(SceUID, taiLoadStartKernelModule, const char *path, SceSize args, Ptr<const void> argp, int flags);
DECL_EXPORT(int, taiStopUnloadKernelModule, SceUID uid, SceSize args, Ptr<const void> argp, int flags, Ptr<int> res);
DECL_EXPORT(int, taiGetModuleInfo, const char *module_name, tai_module_info_t *info);
