// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <host/functions.h>
#include <kernel/thread/thread_functions.h>

#include <psp2/sysmodule.h>

#include <vector>

static constexpr auto SYSMODULE_COUNT = 0x56;

bool is_lle_module(SceSysmoduleModuleId module_id, const std::vector<std::string> &lle_modules);
bool is_module_loaded(KernelState &kernel, SceSysmoduleModuleId module_id);
bool load_module(HostState &host, SceSysmoduleModuleId module_id);
