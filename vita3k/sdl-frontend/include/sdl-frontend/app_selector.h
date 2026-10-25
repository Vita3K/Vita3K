// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <SDL3/SDL_events.h>

#include <cstdint>

struct EmuEnvState;
struct SDL_Window;

namespace sdl_frontend {

struct FrontendState;

bool initialize_app_selector(EmuEnvState &emuenv, SDL_Window *window);
void destroy_app_selector(SDL_Window *window);
void handle_app_selector_event(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window, const SDL_Event &event,
    uint32_t vita_button, bool &running);
void render_app_selector(EmuEnvState &emuenv, SDL_Window *window);
void suspend_app_selector(SDL_Window *window);
bool resume_app_selector(EmuEnvState &emuenv, SDL_Window *window);

} // namespace sdl_frontend