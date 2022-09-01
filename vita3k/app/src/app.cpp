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

#include <config/state.h>
#include <config/version.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <util/log.h>

#include <SDL.h>

namespace app {

void error_dialog(const std::string &message, SDL_Window *window) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window) < 0) {
        LOG_ERROR("SDL Error: {}", message);
    }
}

void calculate_fps(EmuEnvState &emuenv) {
    static const uint32_t frames_size = sizeof(emuenv.fps_values) / sizeof(emuenv.fps_values[0]);
    static uint16_t deltaTimeAcc = 0; // delta time accumulator for the window title

    if (emuenv.frame_count == 0)
        return;

    const uint32_t sdl_ticks_now = SDL_GetTicks();
    const uint32_t deltaTime = sdl_ticks_now - emuenv.sdl_ticks; // Delta time since last frame
    deltaTimeAcc += deltaTime;

    // Only process frames that last AT LEAST 1ms, else math breaks
    // Becuase of this the FPS calculation is maxed out to 1000 FPS (not like any game is gonna hit that)
    // This only happens on some rare cases when the command list is empty and the gpu has to do nothing
    if (deltaTime >= 1) {
        // Set FPS and MS
        emuenv.ms_per_frame = deltaTime;
        emuenv.fps = 1000 / deltaTime;

        emuenv.sdl_ticks = sdl_ticks_now; // Reset deltaTime
        emuenv.frame_count = 0; // 0 Frames to process its FPS

        // Set FPS other fps stats (Min,Max and Avg)
        // Avg
        emuenv.fps_values[emuenv.current_fps_offset] = float(emuenv.fps);
        emuenv.current_fps_offset = (emuenv.current_fps_offset + 1) % frames_size;
        uint32_t avg_fps = 0;
        for (uint32_t i = 0; i < frames_size; i++)
            avg_fps += uint32_t(emuenv.fps_values[i]);
        emuenv.avg_fps = avg_fps / frames_size;
        // Min/Max
        emuenv.min_fps = uint32_t(*std::min_element(emuenv.fps_values, std::next(emuenv.fps_values, frames_size)));
        emuenv.max_fps = uint32_t(*std::max_element(emuenv.fps_values, std::next(emuenv.fps_values, frames_size)));
    }

    if (deltaTimeAcc >= 1000) {
        deltaTimeAcc = 0; // Reset accumulator
        auto old_fps = emuenv.fps; // Save fps value to change it back later

        // Use the average of the last unprocessed frames for the titlebar
        // This is the method that was used before this comment and RPCS3 also uses it
        // Why? its common for games to have lag spike or extra small timeframes in some cases
        // By doing it this way the titlebar won't show them, instead it will show an average
        // of the last unprocessed frames, its accurate enough
        emuenv.fps = 0;
        for (uint32_t i = emuenv.frame_count-1; i >= 0; i--)
            emuenv.fps += emuenv.fps_values[i];
        emuenv.fps /= 2;

        set_window_title(emuenv);
        emuenv.fps = old_fps; // Bring real fps value back
    }
}

void set_window_title(EmuEnvState &emuenv) {
    const auto af = emuenv.cfg.current_config.anisotropic_filtering > 1 ? fmt ::format(" | AF {}x", emuenv.cfg.current_config.anisotropic_filtering) : "";
    const auto x = emuenv.display.frame.image_size.x * emuenv.cfg.current_config.resolution_multiplier;
    const auto y = emuenv.display.frame.image_size.y * emuenv.cfg.current_config.resolution_multiplier;
    const std::string title_to_set = fmt::format("{} | {} ({}) | {} | {} FPS ({} ms) | {}x{}{} {}",
        window_title,
        emuenv.current_app_title, emuenv.io.title_id,
        emuenv.cfg.backend_renderer,
        emuenv.fps, emuenv.ms_per_frame,
        x, y, af, emuenv.cfg.current_config.enable_fxaa ? "| FXAA" : "");

    SDL_SetWindowTitle(emuenv.window.get(), title_to_set.c_str());
}

} // namespace app
