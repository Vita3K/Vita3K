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

#ifdef __ANDROID__

struct SDL_Window;

namespace ime {

// Called once after the SDL window is created to allow text-input APIs to reference it.
void set_sdl_window(SDL_Window *window);

// Show or hide the soft keyboard and toggle the on-screen controller overlay accordingly.
void set_keyboard_active(bool active);

// Push the current native IME snapshot to the Android activity/UI.
void notify_ime_state_changed();

} // namespace ime

#endif // __ANDROID__
