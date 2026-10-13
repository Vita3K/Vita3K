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
#include <sdl-frontend/app_selector.h>
#include <sdl-frontend/functions.h>
#include <sdl/functions.h>
#include <util/exit_code.h>
#include <util/log.h>

#include <SDL3/SDL.h>

namespace sdl_frontend {

MainWindow::~MainWindow() {
    destroy_app_selector(sdl_window);
    if (sdl_window)
        SDL_DestroyWindow(sdl_window);
}

bool MainWindow::initialize(EmuEnvState &emuenv) {
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

    sdl_window = sdl::create_window(window_title, emuenv.backend_renderer, flags);
    if (!sdl_window) {
        LOG_ERROR("SDL window creation failed: {}", SDL_GetError());
        return false;
    }
    if (!sdl::update_viewport(sdl_window, emuenv.cfg.current_config.stretch_the_display_area,
            emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load()))
        return false;
    return true;
}

SDL_Window *MainWindow::window() const {
    return sdl_window;
}

FrontendState &MainWindow::frontend_state() {
    return state;
}

int run_frontend(EmuEnvState &emuenv) {
    if (!app::ensure_current_user(emuenv)) {
        LOG_ERROR("Could not initialize the current user for the SDL frontend.");
        return SDLInitFailed;
    }
    app::load_app_times(emuenv);

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
    const auto sync_discord = [&]() {
        if (!discordrpc::update_init_status(emuenv.cfg.discord_rich_presence, &discord_rich_presence_old))
            return;
        if (session.has_active_session())
            discordrpc::update_presence(emuenv.io.title_id, emuenv.current_app_title);
        else
            discordrpc::update_presence();
    };
    hooks.on_update = sync_discord;
#endif

    if (emuenv.cfg.run_app_path) {
        const auto result = run(emuenv, session, *main_window.window(),
            AppLaunchRequest{ .app_path = *emuenv.cfg.run_app_path }, hooks);
        if (result.window_closed || result.app_exited || result.exit_code != Success)
            return result.exit_code;
        sdl::update_viewport(main_window.window(), emuenv.cfg.current_config.stretch_the_display_area,
            emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load());
    }
    if (!emuenv.cfg.initial_setup && !emuenv.cfg.run_app_path) {
        if (!run_initial_setup(emuenv, main_window.window()))
            return SDLInitFailed;
    }
    auto &state = main_window.frontend_state();
    if (!initialize_app_selector(emuenv, main_window.window()))
        return SDLInitFailed;

    bool running = true;
    while (running) {
#if USE_DISCORD
        sync_discord();
#endif
        auto launch_request = emuenv.take_app_launch_request();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            const auto shared_event = sdl::process_event(emuenv, event);
            if (event.type == SDL_EVENT_DROP_COMPLETE) {
                if (state.screen == FrontendScreen::AppSelector)
                    handle_app_selector_event(emuenv, state, main_window.window(), event, shared_event.vita_button, running);
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && sdl::process_fullscreen_key(emuenv, main_window.window(), shared_event.physical_key))
                sdl::update_viewport(main_window.window(), emuenv.cfg.current_config.stretch_the_display_area,
                    emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load());
            if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
                sdl::update_viewport(main_window.window(), emuenv.cfg.current_config.stretch_the_display_area,
                    emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load());
            if (shared_event.outcome == sdl::EventOutcome::QuitRequested) {
                running = false;
                continue;
            }
            if (!running || launch_request)
                continue;
            if (state.screen == FrontendScreen::UserSelector)
                handle_user_selector_event(emuenv, state, main_window.window(), event, shared_event.vita_button);
            else
                handle_app_selector_event(emuenv, state, main_window.window(), event, shared_event.vita_button, running);
            launch_request = emuenv.take_app_launch_request();
        }

        if (!running)
            return Success;

        if (launch_request) {
            suspend_app_selector(main_window.window());
            const auto result = run(emuenv, session, *main_window.window(),
                std::move(*launch_request), hooks);
            if (result.window_closed)
                return result.exit_code;

            if (result.exit_code != Success && !result.app_exited)
                LOG_ERROR("App launch failed with code {}; returning to the app selector.", result.exit_code);
            sdl::update_viewport(main_window.window(), emuenv.cfg.current_config.stretch_the_display_area,
                emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load());
            sdl::set_default_window_title(main_window.window());
            if (!resume_app_selector(emuenv, main_window.window()))
                return SDLInitFailed;
            continue;
        }

        if (state.screen == FrontendScreen::UserSelector)
            render_user_selector(emuenv);
        else
            render_app_selector(emuenv, main_window.window());
        SDL_Delay(16);
    }
    return Success;
}

} // namespace sdl_frontend