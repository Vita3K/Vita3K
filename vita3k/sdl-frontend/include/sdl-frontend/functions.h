// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <emuenv/app_launch_request.h>
#include <sdl-frontend/state.h>

#include <cstdint>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>

#include <optional>
#include <string>

namespace sdl_frontend {

namespace ui {
void draw_background(SDL_Renderer *renderer, const SDL_FRect &rect, SDL_Color color);
void draw_inset_border(SDL_Renderer *renderer, const SDL_FRect &rect, float inset_x, float inset_y,
    SDL_Color color);
void draw_rect(SDL_Renderer *renderer, const SDL_FRect &rect, SDL_Color color);
void draw_scrollbar(SDL_Renderer *renderer, const SDL_FRect &track, const SDL_FRect &thumb,
    SDL_Color track_color, SDL_Color thumb_color);
void draw_selection_highlight(SDL_Renderer *renderer, const SDL_FRect &rect, SDL_Color fill_color,
    const SDL_Color *outline_color = nullptr);
bool point_in_rect(const SDL_FRect &rect, float x, float y);
bool window_to_render_coordinates(SDL_Renderer *renderer, float window_x, float window_y,
    float &render_x, float &render_y);
} // namespace ui

void destroy_app_selector(SDL_Window *window);
void handle_app_selector_event(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window, const SDL_Event &event,
    uint32_t vita_button, bool &running);
bool initialize_app_selector(EmuEnvState &emuenv, SDL_Window *window);
void render_app_selector(EmuEnvState &emuenv, SDL_Window *window);
bool resume_app_selector(EmuEnvState &emuenv, SDL_Window *window);
std::size_t user_selector_visible_items(float logical_window_width);
void begin_user_selector(EmuEnvState &emuenv);
void reset_user_selector();
void handle_user_selector_event(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window,
    const SDL_Event &event, uint32_t vita_button);
void render_user_selector(EmuEnvState &emuenv);
std::optional<std::string> handle_user_selector_event(EmuEnvState &emuenv, FrontendState &state, SDL_Window *window,
    SDL_Renderer *renderer, const SDL_Event &event, uint32_t vita_button, CoordinateScale coordinate_scale,
    float viewport_x, float viewport_y, float logical_window_width, float logical_window_height);
std::size_t user_selector_item_at(float x, float y, CoordinateScale window_scale, float logical_window_height,
    std::size_t first_item, std::size_t visible_items, std::size_t user_count);
void render_user_selector(EmuEnvState &emuenv, SDL_Renderer *renderer, CoordinateScale coordinate_scale,
    float viewport_x, float viewport_y, float logical_window_width, float logical_window_height);
void destroy_user_selector_icons();
void draw_selector_text(SDL_Renderer *renderer, CoordinateScale coordinate_scale, float x, float y,
    const char *text, SDL_Color color);
// Boots a launch request in a caller-owned window, following in-process relaunches.
// Returns the app exit code, 0 if the window closed, or -1 if the app couldn't start.
SessionResult run(EmuEnvState &emuenv, app::AppSessionController &session, SDL_Window &window,
    AppLaunchRequest launch_request, const Hooks &hooks = {});

int run_frontend(EmuEnvState &emuenv);
bool run_initial_setup(EmuEnvState &emuenv, SDL_Window *window);
void suspend_app_selector(SDL_Window *window);

} // namespace sdl_frontend