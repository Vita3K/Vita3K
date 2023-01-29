// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <imgui.h>
#include <map>
#include <string>

enum CompatibilityState {
    Unknown,
    Nothing = 1260231569, // 0x4b1d9b91
    Bootable = 1344750319, // 0x502742ef
    Intro = 1260231381, // 0x4B9F5E5D
    Menu = 1344751053, // 0x4F1B9135
    Ingame_Less = 1344752299, // 0x4F7B6B3B
    Ingame_More = 1260231985, // 0x4B2A9819
    Playable = 920344019, // 0x36db55d3
};

struct Compatibility {
    uint32_t issue_id;
    CompatibilityState state = Unknown;
    time_t updated_at;
};

struct CompatState {
    std::map<std::string, Compatibility> app_compat_db;
    std::map<CompatibilityState, ImVec4> compat_color{
        { Unknown, ImVec4(0.54f, 0.54f, 0.54f, 1.f) },
        { Nothing, ImVec4(0.70f, 0.13f, 0.13f, 1.f) }, // #b60205
        { Bootable, ImVec4(0.39f, 0.12f, 0.62f, 1.f) }, // #621fa5
        { Intro, ImVec4(0.85f, 0.24f, 0.05f, 1.f) }, // #d93f0b
        { Menu, ImVec4(0.05f, 0.49f, 0.95f, 1.f) }, // #85def2
        { Ingame_Less, ImVec4(0.98f, 0.79f, 0.02f, 1.f) }, // #fbca04
        { Ingame_More, ImVec4(0.98f, 0.79f, 0.02f, 1.f) }, // #fbca04
        { Playable, ImVec4(0.05f, 0.54f, 0.09f, 1.f) }, // #0e8a16
    };
};
