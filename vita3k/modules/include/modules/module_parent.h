// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <kernel/types.h>
#include <module/module.h>
#include <util/types.h>

struct CPUState;
struct EmuEnvState;
struct KernelState;
struct MemState;

void init_libraries(EmuEnvState &emuenv);
void call_import(EmuEnvState &emuenv, CPUState &cpu, uint32_t nid, SceUID thread_id);

/**
 * \brief Loads a dynamic module into memory if it wasn't already loaded. If it was, find it and return it.
 * \param emuenv PlayStation Vita emulated environment
 * \param module_path Full path of module file (with device)
 * \return UID of the loaded module object or SCE_ERROR on failure
 */
SceUID load_module(EmuEnvState &emuenv, const std::string &module_path);

uint32_t start_module(EmuEnvState &emuenv, const std::shared_ptr<SceKernelModuleInfo> &module, SceSize args = 0, const Ptr<void> argp = Ptr<void>{});

/**
 * \brief Loads and run a system module
 * \param emuenv PlayStation Vita emulated environment
 * \param module_id System Module ID of the module to load
 * \return False on failure, true on success
 */
bool load_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id);
bool load_sys_module_internal_with_arg(EmuEnvState &emuenv, SceUID thread_id, SceSysmoduleInternalModuleId module_id, SceSize args, Ptr<void> argp, int *retcode);

int load_app_by_path(EmuEnvState &emuenv, const std::string &self_path, const char *titleid, const char *app_param);
Address resolve_export(KernelState &kernel, uint32_t nid);
uint32_t resolve_nid(KernelState &kernel, Address addr);
Ptr<void> create_vtable(const std::vector<uint32_t> &nids, MemState &mem);

struct VarExport {
    uint32_t nid;
    ImportVarFactory factory;
    const char *name;
};

constexpr int var_exports_size =
#define NID(name, nid)
#define VAR_NID(name, nid) 1 +
#include <nids/nids.inc>
    0;
#undef VAR_NID
#undef NID

const std::array<VarExport, var_exports_size> &get_var_exports();

using LibraryInitFn = std::function<void(EmuEnvState &emuenv)>;

#define LIBRARY_INIT_DECL(name) void export_library_init_##name(EmuEnvState &emuenv);
#define LIBRARY_INIT_IMPL(name) void export_library_init_##name(EmuEnvState &emuenv)
#define LIBRARY_INIT_REGISTER(name) extern const LibraryInitFn import_library_init_##name = export_library_init_##name;