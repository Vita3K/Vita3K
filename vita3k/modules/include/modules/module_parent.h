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

#include <kernel/types.h>
#include <module/bridge.h>
#include <util/types.h>

#include <expected>

struct CPUState;
struct EmuEnvState;
struct KernelModule;
struct KernelState;

void init_libraries(EmuEnvState &emuenv);
void init_exported_vars(EmuEnvState &emuenv);
CallResult call_import(EmuEnvState &emuenv, CPUState &cpu, uint32_t nid, SceUID thread_id);

// Returns true if the NID has an HLE (C++) implementation in nids.inc.
// Used to determine if an LLE export should be overridden with HLE dispatch.
bool has_hle_implementation(uint32_t nid);

/**
 * \brief Loads a dynamic module into memory if it wasn't already loaded. If it was, find it and return it.
 * \param emuenv PlayStation Vita emulated environment
 * \param module_path Full path of module file (with device)
 * \return UID of the loaded module object or SCE_ERROR on failure
 */
SceUID load_module(EmuEnvState &emuenv, const std::string &module_path, uint16_t flags = 0);
int unload_module(EmuEnvState &emuenv, SceUID module_id);

// What a module's start or stop function returned, or the error that kept it from running or that
// its result gives
using ModuleStatus = std::expected<uint32_t, SceInt32>;

// Run a module's module_start or module_stop. They fail for a module in the wrong state, for a start
// or stop that can't run and for one that fails.
ModuleStatus start_module(EmuEnvState &emuenv, KernelModule &module, SceSize args = 0, Ptr<const void> argp = Ptr<const void>{});
ModuleStatus stop_module(EmuEnvState &emuenv, KernelModule &module, SceSize args = 0, Ptr<const void> argp = Ptr<const void>{});

// Load, unload and check a sysmodule id like sysmodule does, for an id the caller has checked. The
// loads and unloads return the error of the module that failed.
SceInt32 load_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id);
SceInt32 unload_sys_module(EmuEnvState &emuenv, SceSysmoduleModuleId module_id);
bool is_sys_module_loaded(EmuEnvState &emuenv, SceSysmoduleModuleId module_id);
SceInt32 load_sys_module_internal_with_arg(EmuEnvState &emuenv, SceSysmoduleInternalModuleId module_id, SceSize args, Ptr<void> argp, int *retcode);
SceInt32 unload_sys_module_internal(EmuEnvState &emuenv, SceSysmoduleInternalModuleId module_id);
bool is_sys_module_internal_loaded(EmuEnvState &emuenv, SceSysmoduleInternalModuleId module_id);

void load_taihen_plugins_for_title(EmuEnvState &emuenv, const std::string &titleid);

Ptr<void> create_vtable(const std::vector<uint32_t> &nids, MemState &mem);
Ptr<void> get_client_vtable(KernelState &kernel, MemState &mem);
