// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <util/log.h>

#include <SDL.h>

namespace app {

void error_dialog(const std::string &message, SDL_Window *window) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window) < 0) {
        LOG_ERROR("SDL Error: {}", message);
    }
}

static const uint32_t frames_size = 20;
void calculate_fps(HostState &host) {
    const uint32_t sdl_ticks_now = SDL_GetTicks();
    const uint32_t ms = sdl_ticks_now - host.sdl_ticks;

    if (ms >= 1000 && host.frame_count > 0) {
        // Set Current FPS
        host.fps = static_cast<std::uint32_t>((host.frame_count * 1000) / ms);
        host.ms_per_frame = ms / static_cast<std::uint32_t>(host.frame_count);
        host.sdl_ticks = sdl_ticks_now;
        host.frame_count = 0;
        set_window_title(host);

        // Set FPS Statistics
        host.fps_values[host.current_fps_offset] = float(host.fps);
        host.current_fps_offset = (host.current_fps_offset + 1) % frames_size;
        uint32_t avg_fps = 0;
        for (uint32_t i = 0; i < frames_size; i++)
            avg_fps += uint32_t(host.fps_values[i]);
        host.avg_fps = avg_fps / frames_size;
        host.min_fps = uint32_t(*std::min_element(host.fps_values, std::next(host.fps_values, frames_size)));
        host.max_fps = uint32_t(*std::max_element(host.fps_values, std::next(host.fps_values, frames_size)));
    }
}

void set_window_title(HostState &host) {
    const std::string title_to_set = fmt::format("{} | {} ({}) | {} ms/frame ({} frames/sec)", window_title,
        host.current_app_title, host.io.title_id, host.ms_per_frame, host.fps);

    SDL_SetWindowTitle(host.window.get(), title_to_set.c_str());
}

} // namespace app
