// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

struct EmuEnvState;
struct SDL_Window;

namespace sdl_frontend {

class MainWindow {
public:
    MainWindow() = default;
    ~MainWindow();

    MainWindow(const MainWindow &) = delete;
    MainWindow &operator=(const MainWindow &) = delete;

    bool initialize(EmuEnvState &emuenv);
    SDL_Window *window() const;

private:
    SDL_Window *sdl_window = nullptr;
};

} // namespace sdl_frontend