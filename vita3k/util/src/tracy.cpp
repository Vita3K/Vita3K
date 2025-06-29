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

#ifdef TRACY_ENABLE
#include <util/log.h>

#include <util/vector_utils.h>

#include <bitset>
#include <vector>

#include <util/tracy_module_utils.h>

namespace tracy_module_utils {

constexpr int max_modules = 128; // If not enough increase to 64*n
typedef std::bitset<max_modules> tracy_module_flags;
typedef std::vector<std::string> tracy_module_names;

static tracy_module_names &get_tracy_available_advanced_profiling_modules() {
    static tracy_module_names tracy_available_advanced_profiling_modules{};
    return tracy_available_advanced_profiling_modules;
}

static tracy_module_flags &get_tracy_advanced_profiling_modules() {
    static tracy_module_flags tracy_advanced_profiling_modules{};
    return tracy_advanced_profiling_modules;
}

tracy_module_helper::tracy_module_helper(const char *module_name) {
    mod_id = get_tracy_available_advanced_profiling_modules().size();
    get_tracy_available_advanced_profiling_modules().emplace_back(module_name);
    if (mod_id >= max_modules) {
        LOG_ERROR_ONCE("Too many tracy modules. Increase max_modules const");
    }
}

bool is_tracy_active(tracy_module_helper module) {
    return get_tracy_advanced_profiling_modules().test(module.mod_id);
}

bool is_tracy_active(const std::string &module) {
    int module_index = vector_utils::find_index(get_tracy_available_advanced_profiling_modules(), module);
    if (module_index >= 0)
        return get_tracy_advanced_profiling_modules().test(module_index);
    else
        return false;
}

void set_tracy_active(const std::string &module, bool value) {
    int module_index = vector_utils::find_index(get_tracy_available_advanced_profiling_modules(), module);
    if (module_index >= 0) {
        get_tracy_advanced_profiling_modules().set(module_index, value);
    }
}

std::vector<std::string> get_available_module_names() {
    std::vector<std::string> names = get_tracy_available_advanced_profiling_modules();
    std::sort(names.begin(), names.end());
    return names;
}

void load_from(const std::vector<std::string> &active_modules_str) {
    get_tracy_advanced_profiling_modules().reset();
    for (auto &module : active_modules_str)
        set_tracy_active(module, true);
}

void cleanup(std::vector<std::string> &active_modules_str) {
    // remove if not found in tracy_available_advanced_profiling_modules
    std::erase_if(active_modules_str, [](const std::string &module) {
        return !vector_utils::contains(get_tracy_available_advanced_profiling_modules(), module);
    });
}

} // namespace tracy_module_utils
#endif
