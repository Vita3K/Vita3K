// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <SDL3/SDL_rect.h>

#include <functional>
#include <string>

namespace app {
class AppSessionController;
}

struct SDL_Window;
struct EmuEnvState;

namespace sdl_frontend {

struct ScrollbarLayout {
    SDL_FRect track;
    SDL_FRect thumb;
    std::size_t max_first_index;
};

struct CoordinateScale {
    float x{ 1.f };
    float y{ 1.f };
};

enum class FrontendScreen {
    AppSelector,
    UserSelector
};

struct FrontendState {
    FrontendScreen screen = FrontendScreen::AppSelector;
};

struct Hooks {
    std::function<void()> on_init;
    std::function<void(SDL_Window *window)> on_window;
    std::function<void(const std::string &app_path)> on_launch;
    std::function<void()> on_update;
    std::function<void()> on_guide_button;
    std::function<void()> on_ime_changed;
    std::function<void()> on_stop;
};

struct SessionResult {
    int exit_code = 0;
    bool window_closed = false;
    bool app_exited = false;
};

} // namespace sdl_frontend