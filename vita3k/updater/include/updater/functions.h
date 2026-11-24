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

#include <optional>
#include <string>
#include <string_view>

namespace updater {

std::string release_api_url();
std::string release_page_url();
std::string display_version(const UpdateInfo &info);
std::string current_display_version();
bool is_official_build();
UpdateCheckResult check_release_response(std::string_view response, std::uint64_t current_build, bool official_build, std::string current_display_version);
UpdateCheckResult check_release_response(std::string_view response, std::string_view current_version, bool official_build);
std::optional<UpdateAsset> find_asset(const UpdateInfo &info, std::string_view expected_name);
bool is_release_api_url(std::string_view url);
bool is_release_asset_url(std::string_view url);
std::string_view update_status_name(UpdateCheckStatus status);
std::string_view update_failure_reason_name(UpdateCheckFailureReason reason);

} // namespace updater
