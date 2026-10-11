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

#include <compat/state.h>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

struct EmuEnvState;

namespace compat {

struct UpdateMessageTexts {
    std::string error_title;
    std::string updated_title;
    std::string check_failed;
    std::string download_failed;
    std::string load_failed;
    std::string new_app_listed;
    std::string app_listed;
    std::string download_app_listed;
};

std::optional<UpdateInfo> parse_ver_resp(const CompatState &state, const std::string &body);
bool load_from_disk(CompatState &state, const std::filesystem::path &cache_path);
bool load_app_compat_db(CompatState &state, EmuEnvState &emuenv, const UpdateMessageTexts &message_texts);
bool update_app_compat_db(CompatState &state, EmuEnvState &emuenv, const UpdateMessageTexts &message_texts);
CompatibilityState get_app_compat(const CompatState &state, const std::string &title_id);
bool install_db(CompatState &state, const std::filesystem::path &cache_path,
    std::span<const uint8_t> zip_data, const std::string &new_version);

} // namespace compat
