// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl-frontend/main_window.h>

#include <app/functions.h>
#include <app/session_controller.h>
#if USE_DISCORD
#include <app/discord.h>
#endif
#include <config/state.h>
#include <config/version.h>
#include <display/state.h>
#include <emuenv/app_launch_request.h>
#include <emuenv/state.h>
#include <io/state.h>
#include <renderer/state.h>
#include <sdl-frontend/functions.h>
#include <sdl/functions.h>
#include <util/exit_code.h>
#include <util/log.h>

#include <SDL3/SDL.h>

namespace sdl_frontend {

void MainWindow::destroy(EmuEnvState &emuenv) {
    emuenv.window.reset();
}

bool MainWindow::initialize(EmuEnvState &emuenv) {
    if (emuenv.window) {
        LOG_ERROR("Cannot initialize the SDL frontend while a window already exists.");
        return false;
    }
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef __ANDROID__
    flags |= SDL_WINDOW_FULLSCREEN;
    emuenv.display.fullscreen = true;
#else
    if (emuenv.cfg.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
        emuenv.display.fullscreen = true;
    }
#endif

    auto *sdl_window = sdl::create_window(window_title, emuenv.backend_renderer, flags);
    if (!sdl_window) {
        LOG_ERROR("SDL window creation failed: {}", SDL_GetError());
        return false;
    }
    emuenv.window = WindowPtr(sdl_window, SDL_DestroyWindow);
    if (!sdl::update_viewport(sdl_window, emuenv.cfg.current_config.stretch_the_display_area,
            emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load())) {
        emuenv.window.reset();
        return false;
    }
    return true;
}

int run_frontend(EmuEnvState &emuenv) {
    if (!emuenv.cfg.run_app_path) {
        LOG_ERROR("The SDL frontend needs an installed app to run, given with -r");
#if defined(_WIN32) && !defined(__ANDROID__)
        sdl::wait_for_console_key();
#endif
        return 1;
    }
    if (!app::ensure_current_user(emuenv)) {
        LOG_ERROR("Could not initialize the current user for the SDL frontend.");
        return SDLInitFailed;
    }

    MainWindow main_window;
#if USE_DISCORD
    struct DiscordShutdownGuard {
        ~DiscordShutdownGuard() {
            discordrpc::shutdown();
        }
    };
    [[maybe_unused]] DiscordShutdownGuard discord_shutdown_guard;
#endif
    if (!main_window.initialize(emuenv))
        return SDLInitFailed;

    app::AppSessionController session(emuenv);
    Hooks hooks{};
#if USE_DISCORD
    bool discord_rich_presence_old = false;
    hooks.on_update = [&]() {
        if (!discordrpc::update_init_status(emuenv.cfg.discord_rich_presence, &discord_rich_presence_old))
            return;
        if (session.has_active_session())
            discordrpc::update_presence(emuenv.io.title_id, emuenv.current_app_title);
        else
            discordrpc::update_presence();
    };
#endif

    const auto result = run(emuenv, session, *emuenv.window,
        AppLaunchRequest{ .app_path = *emuenv.cfg.run_app_path }, hooks);
    main_window.destroy(emuenv);
    return result.exit_code;
}

} // namespace sdl_frontend
