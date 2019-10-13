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

#include <module/load_module.h>

#include <host/load_self.h>
#include <host/state.h>
#include <kernel/state.h>

bool is_lle_module(SceSysmoduleModuleId module_id, const std::vector<std::string> &lle_modules) {
    const auto paths = sysmodule_paths[module_id];

    // Do we know the module and its dependencies' paths?
    const bool have_paths = !paths.empty();

    if (!have_paths)
        return false;

    if (have_paths)
        for (auto path : paths)
            if (std::find(lle_modules.begin(), lle_modules.end(), path) != lle_modules.end())
                return true;

    return false;
}

bool is_module_loaded(KernelState &kernel, SceSysmoduleModuleId module_id) {
    return std::find(kernel.loaded_sysmodules.begin(), kernel.loaded_sysmodules.end(), module_id) != kernel.loaded_sysmodules.end();
}
