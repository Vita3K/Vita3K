// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <vfs/appmeta.h>

#include <util/log.h>

#include <algorithm>

namespace vfs {

static fs::path get_shell_cache_file(const fs::path &vita_fs_path, const std::string &app_path, const std::string &title_id, const fs::path &relative) {
    if (title_id.empty() || title_id.size() > 32 || app_path != "ux0:app/" + title_id
        || !std::all_of(title_id.begin(), title_id.end(), [](unsigned char ch) {
               return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
           }))
        return {};
    const auto ur0 = vita_fs_path / "ur0";
    const auto parent = ur0 / "appmeta";
    const auto appmeta = parent / title_id;
    std::vector<fs::path> components{ ur0, parent, appmeta };
    auto file = appmeta;
    for (const auto &part : relative) {
        file /= part;
        components.push_back(file);
    }
    for (const auto &component : components) {
        std::error_code error;
        const auto status = fs::symlink_status(component, error);
        if (error == std::errc::no_such_file_or_directory || (!error && !fs::exists(status)))
            return {};
        if (error || !fs_utils::is_plain_component(component)
            || (component == file ? !fs::is_regular_file(status) : !fs::is_directory(status))) {
            LOG_ERROR("Cannot resolve shell appmeta resource for '{}': invalid path {}", title_id, component);
            return {};
        }
    }
    return file;
}

fs::path get_app_icon_cache_path(const fs::path &vita_fs_path, const std::string &app_path, const std::string &title_id) {
    return get_shell_cache_file(vita_fs_path, app_path, title_id, "icon0.png");
}

bool read_app_icon(FileBuffer &buf, const fs::path &vita_fs_path, const std::string &app_path, const std::string &title_id) {
    const auto cached = get_app_icon_cache_path(vita_fs_path, app_path, title_id);
    if (!cached.empty()) {
        if (fs_utils::read_data(cached, buf))
            return true;
        LOG_ERROR("Cannot read shell appmeta icon for '{}' at {}", title_id, cached);
        return false;
    }
    return read_app_file(buf, vita_fs_path, app_path, "sce_sys/icon0.png", title_id);
}

bool read_app_livearea_file(FileBuffer &buf, const fs::path &vita_fs_path, const std::string &app_path, const fs::path &vfs_file_path, const std::string &title_id) {
    buf.clear();
    if (vfs_file_path.has_root_path()) {
        LOG_ERROR("Invalid LiveArea resource path for '{}': {}", title_id, vfs_file_path);
        return false;
    }
    auto part = vfs_file_path.begin();
    if (part == vfs_file_path.end() || *part++ != "sce_sys" || part == vfs_file_path.end()) {
        LOG_ERROR("Invalid LiveArea resource path for '{}': {}", title_id, vfs_file_path);
        return false;
    }
    const auto category = *part++;
    bool allowed = category == "icon0.png" || category == "pic0.png";
    if (allowed)
        allowed = part == vfs_file_path.end();
    else if (category == "livearea")
        allowed = part != vfs_file_path.end();
    else if (category == "retail" && part != vfs_file_path.end() && *part++ == "livearea")
        allowed = part != vfs_file_path.end();
    for (const auto &component : vfs_file_path) {
        if (component.empty() || component == "." || component == "..")
            allowed = false;
    }
    if (!allowed) {
        LOG_ERROR("Invalid LiveArea resource path for '{}': {}", title_id, vfs_file_path);
        return false;
    }
    const auto relative = vfs_file_path.lexically_relative("sce_sys");
    const auto cached = get_shell_cache_file(vita_fs_path, app_path, title_id, relative);
    if (!cached.empty()) {
        if (fs_utils::read_data(cached, buf))
            return true;
        LOG_ERROR("Cannot read shell appmeta resource for '{}' at {}", title_id, cached);
        return false;
    }
    return read_app_file(buf, vita_fs_path, app_path, vfs_file_path, title_id);
}

} // namespace vfs