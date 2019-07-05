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

#include <app/functions.h>

#include <config/version.h>
#include <host/state.h>
#include <kernel/functions.h>
#include <util/log.h>

#ifdef USE_DISCORD_RICH_PRESENCE
#include <app/discord.h>
#endif

#include <SDL.h>

#include <cassert>
#include <sstream>

namespace app {

void error_dialog(const std::string &message, SDL_Window *window) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window) < 0) {
        LOG_ERROR("SDL Error: {}", message);
    }
}

void set_window_title(HostState &host) {
    const uint32_t sdl_ticks_now = SDL_GetTicks();
    const uint32_t ms = sdl_ticks_now - host.sdl_ticks;
    if (ms >= 1000 && host.frame_count > 0) {
        const std::uint32_t fps = static_cast<std::uint32_t>((host.frame_count * 1000) / ms);
        const std::uint32_t ms_per_frame = ms / static_cast<std::uint32_t>(host.frame_count);
        std::ostringstream title;
        title << window_title << " | " << host.game_title << " (" << host.io.title_id << ") | " << ms_per_frame << " ms/frame (" << fps << " frames/sec)";
        SDL_SetWindowTitle(host.window.get(), title.str().c_str());
        host.sdl_ticks = sdl_ticks_now;
        host.frame_count = 0;
    }
}

} // namespace app
