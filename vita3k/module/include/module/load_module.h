// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <emuenv/state.h>
#include <kernel/types.h>

#include <array>
#include <vector>

static constexpr auto SYSMODULE_COUNT = 0x56;

using SysmodulePaths = std::array<std::vector<const char *>, SYSMODULE_COUNT>;
using SysmoduleInternalPaths = std::map<SceSysmoduleInternalModuleId, std::vector<const char *>>;

extern const SysmodulePaths sysmodule_paths;
extern const SysmoduleInternalPaths sysmodule_internal_paths;

bool is_lle_module(SceSysmoduleModuleId module_id, EmuEnvState &emuenv);
bool is_lle_module(const std::string &module_name, EmuEnvState &emuenv);
bool is_module_loaded(KernelState &kernel, SceSysmoduleModuleId module_id);
