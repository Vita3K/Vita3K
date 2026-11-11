// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct IOState;

namespace pfsmgr {
bool mount_readonly_appmeta(IOState &io, const std::filesystem::path &vita_fs_path, const std::string &title_id, std::string &mount_point);
bool unmount_appmeta(IOState &io, const std::string &mount_point);
bool copy_appmeta_cache(IOState &io, const std::filesystem::path &vita_fs_path, const std::string &title_id, const std::filesystem::path &destination);
bool mount_readonly_game_data(IOState &io, const std::filesystem::path &vita_fs_path, const std::string &title_id, bool require_patch, std::string &mount_point);
bool unmount_game_data(IOState &io, const std::string &mount_point);
bool copy_appmeta(IOState &io, const std::filesystem::path &vita_fs_path, const std::string &title_id,
    const std::filesystem::path &destination, const std::filesystem::path &existing_appmeta, bool require_patch,
    std::vector<uint8_t> *self_header = nullptr);
bool read_update_history(IOState &io, const std::filesystem::path &vita_fs_path, const std::string &app_path,
    const std::string &title_id, int language, std::vector<uint8_t> &xml);
bool read_manual_pages(IOState &io, const std::filesystem::path &vita_fs_path, const std::string &app_path,
    const std::string &title_id, int language, const std::function<bool(const std::vector<uint8_t> &, unsigned int)> &consume_page);
bool mount_application_game_data(IOState &io, const std::filesystem::path &vita_fs_path);
int mount_addcont(IOState &io, const std::filesystem::path &vita_fs_path, const std::string &mount_point, const std::string &entitlement);
int unmount_addcont(IOState &io, const std::string &mount_point, const std::string &entitlement);
bool mount_theme_data(IOState &io, const std::filesystem::path &directory, std::string &mount_point, bool &is_protected);
bool unmount_theme_data(IOState &io, const std::string &mount_point);
bool copy_theme_data(IOState &io, const std::filesystem::path &vita_fs_path, const std::string &content_id, const std::filesystem::path &destination);
void clear_shell_mounts(IOState &io);
void clear_application_mounts(IOState &io);
} // namespace pfsmgr