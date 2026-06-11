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

#include <config/state.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace config {

enum class RestartRequiredSetting : uint8_t {
    CpuOpt = 0,
    BackendRenderer = 1,
    GraphicsDevice = 2,
    CustomDriver = 3,
    HighAccuracy = 4,
    ResolutionMultiplier = 5,
    MemoryMapping = 6,
    AudioBackend = 7,
    ValidationLayer = 8,
};

std::vector<RestartRequiredSetting> get_restart_required_settings(
    const Config::CurrentConfig &before,
    const Config::CurrentConfig &after);

bool load_custom_config(Config::CurrentConfig &out, const fs::path &config_path, const std::string &app_path);
bool save_custom_config(const Config::CurrentConfig &cc, const fs::path &config_path, const std::string &app_path);
bool delete_custom_config(const fs::path &config_path, const std::string &app_path);
int delete_all_custom_configs(const fs::path &config_path);
bool has_custom_config(const fs::path &config_path, const std::string &app_path);
void set_current_config(Config &cfg, const fs::path &config_path, const std::string &app_path);
void copy_current_config_to_global(Config &cfg);
void save_current_config(Config &cfg, const fs::path &config_path, const std::string &app_path, bool create_custom_if_missing = false);
std::vector<std::pair<std::string, bool>> get_modules_list(
    const fs::path &vita_fs_path,
    const std::vector<std::string> &lle_modules);

} // namespace config
