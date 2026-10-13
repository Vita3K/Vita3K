// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <vita_shell/state.h>

namespace vita_shell {

void load_theme(const std::filesystem::path &vita_fs_path,
    const std::filesystem::path &static_assets_path, const std::string &content_id);
const ThemeState &get_theme_state();
ThemeState load_theme_directory(const std::filesystem::path &theme_directory,
    const std::string &preferred_locale = {});
std::vector<ThemeState> enumerate_themes(const std::filesystem::path &themes_root,
    const std::string &preferred_locale = {});

} // namespace vita_shell