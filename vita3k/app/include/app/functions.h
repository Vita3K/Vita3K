// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <gui/imgui_impl_sdl_state.h>

#include <string>

struct Config;
struct HostState;
struct SDL_Window;
template <class T>
class Ptr;
class Root;

namespace app {

/// Describes the state of the application to be run
enum class AppRunType {
    /// Run type is unknown
    Unknown,
    /// Extracted, files are as they are on console
    Extracted,
    /// Zipped in HENKaku-style .vpk file
    Vpk,
};

bool init(HostState &state, Config cfg, const Root &root_paths);
void destroy(HostState &host, ImGui_State *imgui);
void update_viewport(HostState &state);
void error_dialog(const std::string &message, SDL_Window *window = nullptr);

void set_window_title(HostState &host);
void calculate_fps(HostState &host);

} // namespace app
