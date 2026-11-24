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

#include <SDL3/SDL_events.h>

namespace sdl_frontend {

// Boots a launch request in a caller-owned window, following in-process relaunches.
// Returns the app exit code, 0 if the window closed, or -1 if the app couldn't start.
SessionResult run(EmuEnvState &emuenv, app::AppSessionController &session, SDL_Window &window,
    AppLaunchRequest launch_request, const Hooks &hooks = {});
int run_frontend(EmuEnvState &emuenv);

} // namespace sdl_frontend
