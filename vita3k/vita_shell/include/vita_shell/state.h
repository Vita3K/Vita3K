// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace vita_shell {

struct Color {
    uint8_t r{ 255 };
    uint8_t g{ 255 };
    uint8_t b{ 255 };
    uint8_t a{ 255 };
};

struct ThemeBackground {
    std::string name;
    std::filesystem::path path;
    std::optional<Color> text_color;
};

enum class NoticeIcon : uint8_t {
    NoNotice,
    NewNotice
};

struct InformationBarState {
    std::optional<Color> bar_color;
    std::optional<Color> indicator_color;
    std::optional<Color> notice_font_color;
    std::map<NoticeIcon, std::filesystem::path> notice_icons;
};

enum class StartDateLayout : int32_t {
    LeftDown,
    LeftUp,
    RightDown
};

struct ThemeBgm {
    std::string device{ "pd0" };
    std::filesystem::path device_path{ "data/systembgm/home.at9" };
    std::filesystem::path path;
};

struct ThemeState {
    bool loaded{};
    std::string content_id;
    std::string name;
    std::string provider;
    std::string content_version;
    std::filesystem::path installed_path;
    std::filesystem::path package_thumbnail_path;
    std::vector<ThemeBackground> backgrounds;
    std::map<std::string, std::filesystem::path> system_app_icons;
    ThemeBgm bgm;
    Color text_color{};
    InformationBarState information_bar;
    std::filesystem::path start_screen_background;
    std::optional<Color> start_date_color;
    std::optional<Color> start_notice_background_color;
    std::optional<Color> start_notice_border_color;
    std::optional<Color> start_notice_font_color;
    StartDateLayout start_date_layout{ StartDateLayout::LeftDown };
    std::uintmax_t installed_size{};
    std::time_t updated_time{};
};

} // namespace vita_shell