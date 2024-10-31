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

#include <util/fs.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace patch_check {

enum class HistoryHeadingLevel {
    None = 0,
    H1,
    H2,
    H3,
    H4,
    H5,
    H6
};

struct UpdateHistoryLine {
    std::string color;
    uint32_t font_size = 3;
    HistoryHeadingLevel heading_level = HistoryHeadingLevel::None;
    bool in_list = false;
    bool is_bullet = false;
    std::string text;
};

struct RemoteUpdateInfo {
    std::string titleid;
    std::string version;
    size_t size = 0;
    std::string url;
    std::string content_id;
    std::string title;
};

enum class UpdateState {
    NONE,
    DOWNLOADING,
    WAITING_INSTALL,
    INSTALLING,
    SUCCESS,
    FAILED
};

struct UpdateInstall {
    std::string content_id;
    fs::path pkg_path;
    UpdateState state = UpdateState::NONE;
};

struct PatchCheckState {
public:
    PatchCheckState() = default;
    ~PatchCheckState() = default;

    void erase_cached_update_info(const std::string &app_path);
    RemoteUpdateInfo *find_cached_update_info(const std::string &app_path);
    UpdateInstall *find_update_install(const std::string &app_path);
    bool *find_app_has_update(const std::string &app_path);
    bool has_app_update(const std::string &app_path);
    void set_app_has_update(const std::string &app_path, bool has_update);
    void erase_app_has_update(const std::string &app_path);
    bool sync_app_has_update_from_local(const fs::path &pref_path, const std::string &app_path, const std::string &title_id, const std::string &current_version);
    UpdateInstall &begin_update_download(const std::string &app_path);
    UpdateInstall &queue_update_install(const std::string &app_path, const std::string &content_id, const fs::path &pkg_path);
    void mark_update_install_result(const std::string &app_path, bool success, bool erase_on_failure);
    void erase_update_install(const std::string &app_path);
    void set_update_install_state(const std::string &app_path, UpdateState state_value);
    bool has_update_install_state(const std::string &app_path, UpdateState state_value);
    void cache_remote_update_info(const std::string &app_path, RemoteUpdateInfo info);

private:
    std::mutex mutex;
    std::map<std::string, std::shared_ptr<RemoteUpdateInfo>> new_update_infos;
    std::map<std::string, std::shared_ptr<bool>> app_has_update;
    std::map<std::string, std::shared_ptr<UpdateInstall>> updates_install;
};

} // namespace patch_check
