// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl/sdl_title.h>

#include <config/state.h>
#include <config/version.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <io/state.h>

#include <fmt/format.h>

namespace sdl {

void set_default_window_title(SDL_Window *window) {
    SDL_SetWindowTitle(window, window_title);
}

void set_loading_window_title(SDL_Window *window, const EmuEnvState &emuenv) {
    const auto title = fmt::format("{} | {} ({}) | Please wait, loading...",
        window_title, emuenv.current_app_title, emuenv.io.title_id);
    SDL_SetWindowTitle(window, title.c_str());
}

void update_runtime_window_title(SDL_Window *window, const EmuEnvState &emuenv) {
    const auto &current_config = emuenv.cfg.current_config;
    const auto anisotropic_filtering = current_config.anisotropic_filtering > 1
        ? fmt::format(" | AF {}x", current_config.anisotropic_filtering)
        : "";
    const auto width = emuenv.display.next_rendered_frame.image_size.x * current_config.resolution_multiplier;
    const auto height = emuenv.display.next_rendered_frame.image_size.y * current_config.resolution_multiplier;
    const auto title = fmt::format("{} | {} ({}) | {} | {} FPS ({} ms) | {}x{}{} | {}",
        window_title, emuenv.current_app_title, emuenv.io.title_id,
        current_config.backend_renderer,
        emuenv.fps, emuenv.ms_per_frame,
        width, height, anisotropic_filtering, current_config.screen_filter);
    SDL_SetWindowTitle(window, title.c_str());
}

} // namespace sdl
