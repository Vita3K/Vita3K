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

#include <updater/state.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace updater {

std::string release_api_url();
std::string release_page_url();
std::string display_version(const UpdateInfo &info);
std::string current_display_version();
bool is_official_build();

// Path of the AppImage Vita3K is running from, empty when it is not running from one.
std::string appimage_path();

// Continuous release asset for the running platform, empty when Vita3K cannot replace itself.
std::string release_asset_name();
std::string release_asset_url();
bool can_self_update();

// Unpacks a downloaded release over install_dir, renaming the files it replaces so the running build keeps working.
bool install_update(std::span<const uint8_t> archive_data, const std::filesystem::path &install_dir, std::string &error_message);

// Deletes the renamed files a previous install_update call left behind.
void remove_update_backups(const std::filesystem::path &install_dir);

} // namespace updater
