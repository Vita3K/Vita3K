// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <sdl-frontend/state.h>

struct EmuEnvState;
struct SDL_Window;

namespace sdl_frontend {

class MainWindow {
public:
    MainWindow() = default;

    MainWindow(const MainWindow &) = delete;
    MainWindow &operator=(const MainWindow &) = delete;

    bool initialize(EmuEnvState &emuenv);
    void destroy(EmuEnvState &emuenv);
    FrontendState &frontend_state();

private:
    FrontendState state;
};

int run_frontend(EmuEnvState &emuenv);

} // namespace sdl_frontend