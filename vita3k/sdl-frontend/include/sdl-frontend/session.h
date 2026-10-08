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

#include <emuenv/app_launch_request.h>

#include <functional>
#include <string>

struct EmuEnvState;
struct SDL_Window;

namespace app {
class AppSessionController;
}

namespace sdl_frontend {

// What a platform adds around an SDL session. Every hook is optional.
struct Hooks {
    // SDL is initialized and the app is about to boot
    std::function<void()> on_init;
    // The window was created, or is about to be destroyed when it is null
    std::function<void(SDL_Window *window)> on_window;
    // The app was found and starts booting
    std::function<void(const std::string &app_path)> on_launch;
    // The gamepad's guide button was pressed
    std::function<void()> on_guide_button;
    // The text or state of the active IME changed
    std::function<void()> on_ime_changed;
    // The session is about to stop
    std::function<void()> on_stop;
};

// Boots launch_request in an SDL window and runs it until the app exits or the window is closed,
// following in-process relaunches. Returns 0, or -1 if the app couldn't start.
int run(EmuEnvState &emuenv, app::AppSessionController &session, AppLaunchRequest launch_request, const Hooks &hooks = {});

} // namespace sdl_frontend
