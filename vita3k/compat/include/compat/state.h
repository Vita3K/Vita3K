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
    Unknown = -1,
    Nothing,
    Bootable,
    Intro,
    Menu,
    Ingame_Less,
    Ingame_More,
    Playable,
};

struct Compatibility {
    uint32_t issue_id;
    CompatibilityState state = Unknown;
    time_t updated_at;
};

struct CompatState {
    bool compat_db_loaded = false;
    std::map<std::string, Compatibility> app_compat_db;
    std::map<CompatibilityState, ImVec4> compat_color{
        { Unknown, ImVec4(0.54f, 0.54f, 0.54f, 1.f) },
        { Nothing, ImVec4(1.00f, 0.00f, 0.00f, 1.f) }, // #ff0000
        { Bootable, ImVec4(0.39f, 0.12f, 0.62f, 1.f) }, // #621fa5
        { Intro, ImVec4(0.77f, 0.08f, 0.52f, 1.f) }, // #c71585
        { Menu, ImVec4(0.11f, 0.46f, 0.85f, 1.f) }, // #1d76db
        { Ingame_Less, ImVec4(0.88f, 0.54f, 0.12f, 1.f) }, // #e08a1e
        { Ingame_More, ImVec4(1.00f, 0.84f, 0.00f, 1.f) }, // #ffd700
        { Playable, ImVec4(0.05f, 0.54f, 0.09f, 1.f) }, // #0e8a16
    };
};
