// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <io/vfs.h>

namespace vfs {

fs::path get_app_icon_cache_path(const fs::path &vita_fs_path, const std::string &app_path, const std::string &title_id);
bool read_app_icon(FileBuffer &buf, const fs::path &vita_fs_path, const std::string &app_path, const std::string &title_id);
bool read_app_livearea_file(FileBuffer &buf, const fs::path &vita_fs_path, const std::string &app_path, const fs::path &vfs_file_path, const std::string &title_id);

} // namespace vfs