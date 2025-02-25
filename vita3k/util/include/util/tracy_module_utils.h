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

#include <string>
#include <vector>

namespace tracy_module_utils {

// Helper struct to register module names on application startup. It is used in TRACY_MODULE_NAME macro
struct tracy_module_helper {
    int mod_id;
    tracy_module_helper(const char *module_name);
};

// function for tracy macro to check if module is active
bool is_tracy_active(tracy_module_helper module);

// functions for settings dialog
std::vector<std::string> get_available_module_names();
bool is_tracy_active(const std::string &module);
void set_tracy_active(const std::string &module, bool value);
// function for config loading
void load_from(const std::vector<std::string> &active_modules_str);
// remove non-existent modules
void cleanup(std::vector<std::string> &active_modules_str);
} // namespace tracy_module_utils
