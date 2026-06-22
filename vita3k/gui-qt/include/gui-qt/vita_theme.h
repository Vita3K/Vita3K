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
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace gui {

enum class VitaThemeBackgroundSource {
    Home,
    Lockscreen,
};

struct VitaThemeColor {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;
};

struct VitaThemeBackgroundOption {
    std::string id;
    std::string label;
    fs::path asset_path;
    VitaThemeBackgroundSource source = VitaThemeBackgroundSource::Home;
    std::optional<VitaThemeColor> primary_text_color;
};

struct VitaThemePalette {
    std::optional<VitaThemeColor> information_bar_color;
    std::optional<VitaThemeColor> information_indicator_color;
    std::optional<VitaThemeColor> notify_background_color;
    std::optional<VitaThemeColor> notify_border_color;
    std::optional<VitaThemeColor> notify_font_color;
    std::optional<VitaThemeColor> date_color;
};

struct VitaThemeInfo {
    std::string theme_id;
    std::string content_id;
    std::string title;
    std::string provider;
    std::string content_version;
    fs::path installed_path;
    std::optional<fs::path> bgm_path;
    fs::path package_thumbnail_path;
    std::uintmax_t installed_size = 0;
    std::time_t updated_time = 0;
    std::vector<VitaThemeBackgroundOption> background_options;
    VitaThemePalette palette;
};

std::optional<VitaThemeInfo> load_vita_theme(
    const fs::path &theme_dir,
    const std::string &preferred_locale = {});
std::vector<VitaThemeInfo> enumerate_installed_vita_themes(
    const fs::path &themes_root,
    const std::string &preferred_locale = {});

std::optional<fs::path> synthesize_vita_theme_qss(
    const VitaThemeInfo &theme,
    const std::string &background_option_id,
    int readability_percent,
    bool normalize_font_colors,
    const std::vector<fs::path> &background_image_paths,
    int cycle_interval_seconds,
    bool cycle_enabled,
    const fs::path &vita_fs_path,
    const fs::path &gui_settings_dir);

} // namespace gui
