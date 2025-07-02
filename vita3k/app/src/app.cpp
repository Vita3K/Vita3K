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

#include <app/functions.h>

#include <config/state.h>
#include <config/version.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <util/log.h>

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_timer.h>

namespace app {

void error_dialog(const std::string &message, SDL_Window *window) {
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window)) {
        LOG_ERROR("SDL Error: {}", message);
    }
}

static constexpr uint32_t frames_size = 20;
void calculate_fps(EmuEnvState &emuenv) {
    const uint32_t sdl_ticks_now = SDL_GetTicks();
    const uint32_t ms = sdl_ticks_now - emuenv.sdl_ticks;

    if (ms >= 1000 && emuenv.frame_count > 0) {
        // Set Current FPS
        // round division to nearest integer instead of truncating
        const uint32_t frame_count = static_cast<std::uint32_t>(emuenv.frame_count);
        emuenv.fps = (frame_count * 1000 + ms / 2) / ms;
        emuenv.ms_per_frame = (ms + frame_count / 2) / frame_count;
        emuenv.sdl_ticks = sdl_ticks_now;
        emuenv.frame_count = 0;
        set_window_title(emuenv);

        // Set FPS Statistics
        emuenv.fps_values[emuenv.current_fps_offset] = static_cast<float>(emuenv.fps);
        emuenv.current_fps_offset = (emuenv.current_fps_offset + 1) % frames_size;
        float avg_fps = 0;
        for (uint32_t i = 0; i < frames_size; i++)
            avg_fps += emuenv.fps_values[i];
        emuenv.avg_fps = static_cast<uint32_t>(avg_fps) / frames_size;
        emuenv.min_fps = static_cast<uint32_t>(*std::min_element(emuenv.fps_values, std::next(emuenv.fps_values, frames_size)));
        emuenv.max_fps = static_cast<uint32_t>(*std::max_element(emuenv.fps_values, std::next(emuenv.fps_values, frames_size)));
    }
}

void set_window_title(EmuEnvState &emuenv) {
    const auto af = emuenv.cfg.current_config.anisotropic_filtering > 1 ? fmt ::format(" | AF {}x", emuenv.cfg.current_config.anisotropic_filtering) : "";
    const auto x = emuenv.display.next_rendered_frame.image_size.x * emuenv.cfg.current_config.resolution_multiplier;
    const auto y = emuenv.display.next_rendered_frame.image_size.y * emuenv.cfg.current_config.resolution_multiplier;
    const std::string title_to_set = fmt::format("{} | {} ({}) | {} | {} FPS ({} ms) | {}x{}{} | {}",
        window_title,
        emuenv.current_app_title, emuenv.io.title_id,
        emuenv.cfg.backend_renderer,
        emuenv.fps, emuenv.ms_per_frame,
        x, y, af, emuenv.cfg.current_config.screen_filter);

    SDL_SetWindowTitle(emuenv.window.get(), title_to_set.c_str());
}

} // namespace app
