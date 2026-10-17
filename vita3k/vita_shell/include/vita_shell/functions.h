// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <memory>
#include <optional>
#include <vita_shell/state.h>

struct IOState;
struct RegMgrState;

namespace vita_shell {

class ThemeReader;

std::optional<ThemeCatalog> build_theme_catalog(const std::filesystem::path &vita_fs_path, int language,
    const std::string &default_title, IOState &io);

void load_theme(const std::filesystem::path &vita_fs_path,
    const std::filesystem::path &static_assets_path, const std::string &content_id);
ThemeState prepare_theme(const std::filesystem::path &vita_fs_path,
    const std::filesystem::path &static_assets_path, const std::string &content_id);
void clear_theme();
const ThemeState &get_theme_state();
ThemeState load_theme_directory(const std::filesystem::path &theme_directory,
    const std::string &preferred_locale = {});
std::vector<ThemeState> enumerate_themes(const std::filesystem::path &themes_root,
    const std::string &preferred_locale = {});

class ThemeReader {
public:
    explicit ThemeReader(const std::filesystem::path &directory, bool read_pfs, IOState *io = nullptr);
    ~ThemeReader();
    ThemeReader(const ThemeReader &) = delete;
    ThemeReader &operator=(const ThemeReader &) = delete;
    bool read(const std::filesystem::path &relative_path, std::vector<uint8_t> &data) const;

private:
    std::filesystem::path directory_;
    IOState *io_ = nullptr;
    std::string mount_point_;
    bool owns_mount_ = false;
    bool protected_ = false;
    bool valid_ = true;
};

bool apply_shell_theme(IOState &io, RegMgrState &regmgr, const std::filesystem::path &vita_fs_path, const std::string &content_id);
bool set_shell_theme_registry(RegMgrState &regmgr, const std::string &content_id);

} // namespace vita_shell