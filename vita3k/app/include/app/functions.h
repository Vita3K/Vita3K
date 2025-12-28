// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <string>

struct Config;
struct EmuEnvState;
struct SDL_Window;
struct ImGui_State;
class Root;

namespace app {

/// Describes the state of the application to be run
enum class AppRunType {
    /// Run type is unknown
    Unknown,
    /// Extracted, files are as they are on console
    Extracted,
};

void init_paths(Root &root_paths);
bool init(EmuEnvState &state, Config &cfg, const Root &root_paths);
bool late_init(EmuEnvState &state);
void destroy(EmuEnvState &emuenv, ImGui_State *imgui);
void update_viewport(EmuEnvState &state);
void switch_state(EmuEnvState &emuenv, const bool pause);
void error_dialog(const std::string &message, SDL_Window *window = nullptr);

#ifdef __ANDROID__
void add_custom_driver(EmuEnvState &emuenv);
void remove_custom_driver(EmuEnvState &emuenv, const std::string &driver);
#endif

void set_window_title(EmuEnvState &emuenv);
void calculate_fps(EmuEnvState &emuenv);

} // namespace app
