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

#pragma once

#include <kernel/types.h>
#include <module/bridge.h>
#include <util/types.h>

struct CPUState;
struct EmuEnvState;
struct KernelState;

void init_libraries(EmuEnvState &emuenv);
void init_exported_vars(EmuEnvState &emuenv);
void call_import(EmuEnvState &emuenv, CPUState &cpu, uint32_t nid, SceUID thread_id);

/**
 * \brief Loads a dynamic module into memory if it wasn't already loaded. If it was, find it and return it.
 * \param emuenv PlayStation Vita emulated environment
 * \param module_path Full path of module file (with device)
 * \return UID of the loaded module object or SCE_ERROR on failure
 */
SceUID load_module(EmuEnvState &emuenv, const std::string &module_path);
int unload_module(EmuEnvState &emuenv, SceUID module_id);
SceUID load_app_by_path(EmuEnvState &emuenv, const std::string &self_path, const char *titleid, const char *app_param);

uint32_t start_module(EmuEnvState &emuenv, const SceKernelModuleInfo &module, SceSize args = 0, Ptr<const void> argp = Ptr<const void>{});
uint32_t stop_module(EmuEnvState &emuenv, const SceKernelModuleInfo &module, SceSize args = 0, Ptr<const void> argp = Ptr<const void>{});

/**
 * \brief Loads and run a system module
 * \param emuenv PlayStation Vita emulated environment
 * \param module_id System Module ID of the module to load
 * \return False on failure, true on success
 */
bool load_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id);
int unload_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id);
bool load_sys_module_internal_with_arg(EmuEnvState &emuenv, SceUID thread_id, SceSysmoduleInternalModuleId module_id, SceSize args, Ptr<void> argp, int *retcode);

Ptr<void> create_vtable(const std::vector<uint32_t> &nids, MemState &mem);
Ptr<void> get_client_vtable(MemState &mem);
