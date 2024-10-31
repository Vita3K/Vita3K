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

#include <patch_check/state.h>

#include <vector>

namespace patch_check {

PatchCheckState &get_state();
bool has_patch(const fs::path &pref_path, const std::string &title_id, const std::string &current_version);
bool refresh_update_xml(const fs::path &pref_path, const std::string &title_id);
bool refresh_app_update_state(const fs::path &pref_path, const std::string &app_path, const std::string &title_id, const std::string &current_version);
bool cache_remote_update_info(const fs::path &pref_path, const std::string &app_path, const std::string &title_id, const std::string &current_version, uint32_t sys_lang);
RemoteUpdateInfo *ensure_cached_remote_update_info(const fs::path &pref_path, const std::string &app_path, const std::string &title_id, const std::string &current_version, uint32_t sys_lang);
bool get_remote_update_info(const fs::path &pref_path, const std::string &title_id, const std::string &current_version, uint32_t sys_lang, RemoteUpdateInfo &info);
bool get_update_history(const fs::path &pref_path, const std::string &title_id, const std::string &current_version, uint32_t sys_lang, std::map<std::string, std::vector<UpdateHistoryLine>> &history_lines, bool &is_new_version);
bool get_local_update_history(const fs::path &pref_path, const std::string &app_path, uint32_t sys_lang, std::map<std::string, std::vector<UpdateHistoryLine>> &history_lines, bool &is_new_version);
bool parse_update_history(const std::string &update_history_xml, const std::string &current_version, std::map<std::string, std::vector<UpdateHistoryLine>> &history_lines, bool &is_new_version);

} // namespace patch_check
