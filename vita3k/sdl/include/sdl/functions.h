// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <sdl/state.h>

#include <SDL3/SDL_video.h>

struct EmuEnvState;

namespace sdl {

#if defined(_WIN32) && !defined(__ANDROID__)
void init_console();
#endif

bool initialize(bool has_video);

EventResult process_event(EmuEnvState &emuenv, const SDL_Event &event);
void discard_pending_drop_files();
bool set_fullscreen(EmuEnvState &emuenv, SDL_Window *window, bool fullscreen);
bool process_fullscreen_key(EmuEnvState &emuenv, SDL_Window *window, input::PhysicalKeyCode key);
void clear_keyboard_state(EmuEnvState &emuenv);

SDL_Window *create_window(const char *title, renderer::Backend backend, SDL_WindowFlags flags = 0);
bool update_viewport(SDL_Window *window, bool stretch_display_area, bool fullscreen_hd_res_pixel_perfect,
    bool fullscreen);
const Viewport &get_viewport();
void set_default_window_title(SDL_Window *window);
void set_loading_window_title(SDL_Window *window, const EmuEnvState &emuenv);
void update_runtime_window_title(SDL_Window *window, const EmuEnvState &emuenv);

} // namespace sdl