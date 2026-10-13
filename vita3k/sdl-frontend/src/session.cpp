// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <sdl-frontend/functions.h>
#include <sdl/frame_host.h>
#include <sdl/functions.h>
#include <sdl/sdl_event.h>
#include <sdl/sdl_title.h>
#include <sdl/sdl_window.h>

#include <app/functions.h>
#include <app/session_controller.h>
#include <app/state.h>
#include <audio/state.h>
#include <bgm_player/functions.h>
#include <config/state.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <ime/functions.h>
#include <io/state.h>
#include <motion/event_handler.h>
#include <renderer/state.h>
#include <touch/functions.h>
#include <touch/state.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL3/SDL.h>

#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
namespace sdl_frontend {

// SDL_Quit at the end of a session stops the audio device, so the next session opens a new one
static void reset_session_audio(EmuEnvState &emuenv) {
    if (!emuenv.audio.adapter)
        return;

    LOG_DEBUG("Resetting audio backend '{}' after SDL session shutdown", emuenv.audio.audio_backend);
    emuenv.audio.adapter.reset();
    emuenv.audio.audio_backend.clear();
}

static void update_keyboard(EmuEnvState &emuenv, SDL_Window *window) {
    const bool active = window && SDL_GetKeyboardFocus() == window && !is_any_ime_active(emuenv);
    int key_count = 0;
    const bool *keys = active ? SDL_GetKeyboardState(&key_count) : nullptr;
    update_keyboard_state(emuenv, [&](input::PhysicalKeyCode key) {
        const uint32_t scancode = input::physical_key_to_sdl_scancode(key);
        return keys && key_count > 0 && scancode != SDL_SCANCODE_UNKNOWN
            && scancode < static_cast<uint32_t>(key_count) && keys[scancode];
    });
}

static void update_mouse_input(EmuEnvState &emuenv, SDL_Window *window) {
    auto &touch = emuenv.touch;
    auto &overlay = emuenv.ctrl.overlay_mouse;
    if (!window) {
        touch.renderer_focused = false;
        touch.mouse_button_left = false;
        touch.mouse_button_right = false;
        overlay.pressed.store(false, std::memory_order_relaxed);
        return;
    }

#ifdef __ANDROID__
    return;
#else
    if (touch.finger_count > 0)
        return;

    float window_x = 0.f;
    float window_y = 0.f;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&window_x, &window_y);
    int window_width = 0;
    int window_height = 0;
    int drawable_width = 0;
    int drawable_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);
    SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);

    const float scale_x = window_width > 0 && drawable_width > 0
        ? static_cast<float>(drawable_width) / static_cast<float>(window_width)
        : 1.f;
    const float scale_y = window_height > 0 && drawable_height > 0
        ? static_cast<float>(drawable_height) / static_cast<float>(window_height)
        : 1.f;
    const bool focused = SDL_GetKeyboardFocus() == window || SDL_GetMouseFocus() == window;

    touch.mouse_x = window_x * scale_x;
    touch.mouse_y = window_y * scale_y;
    touch.renderer_focused = focused;
    touch.mouse_button_left = focused && (buttons & SDL_BUTTON_LMASK) != 0;
    touch.mouse_button_right = focused && (buttons & SDL_BUTTON_RMASK) != 0;

    overlay.x.store(window_width > 0 ? window_x * 960.f / window_width : window_x,
        std::memory_order_relaxed);
    overlay.y.store(window_height > 0 ? window_y * 544.f / window_height : window_y,
        std::memory_order_relaxed);
    overlay.pressed.store(touch.mouse_button_left, std::memory_order_relaxed);
#endif
}

static void notify_ime_changed(const Hooks &hooks) {
    if (hooks.on_ime_changed)
        hooks.on_ime_changed();
}

// Returns whether the key was for the active IME
static bool handle_ime_keydown(EmuEnvState &emuenv, const Hooks &hooks, const SDL_KeyboardEvent &event) {
    if (!is_any_ime_active(emuenv))
        return false;

    auto &ime = emuenv.ime;
    switch (event.key) {
    case SDLK_BACKSPACE: {
        const std::lock_guard<std::mutex> lock(ime.mutex);
        ime_backspace(ime);
        break;
    }
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        submit_current_ime(emuenv);
        break;
    case SDLK_ESCAPE:
        dismiss_current_ime(emuenv);
        break;
    case SDLK_LEFT: {
        const std::lock_guard<std::mutex> lock(ime.mutex);
        ime_cursor_left(ime);
        break;
    }
    case SDLK_RIGHT: {
        const std::lock_guard<std::mutex> lock(ime.mutex);
        ime_cursor_right(ime);
        break;
    }
    default:
        return false;
    }

    notify_ime_changed(hooks);
    return true;
}

static void handle_ime_text_editing(EmuEnvState &emuenv, const Hooks &hooks, const char *text) {
    if (!is_any_ime_active(emuenv))
        return;

    {
        const std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        ime_set_preedit(emuenv.ime, string_utils::utf8_to_utf16(text ? text : ""));
    }
    notify_ime_changed(hooks);
}

static void handle_ime_text_input(EmuEnvState &emuenv, const Hooks &hooks, const char *text) {
    if (!is_any_ime_active(emuenv) || !text || text[0] == '\0')
        return;

    std::string filtered_text;
    filtered_text.reserve(std::strlen(text));
    for (const char *ch = text; *ch != '\0'; ++ch) {
        if (*ch != '\n' && *ch != '\r')
            filtered_text.push_back(*ch);
    }

    if (filtered_text.empty())
        return;

    {
        const std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        ime_commit_text(emuenv.ime, string_utils::utf8_to_utf16(filtered_text));
    }
    notify_ime_changed(hooks);
}

SessionResult run(EmuEnvState &emuenv, app::AppSessionController &session, SDL_Window &window, AppLaunchRequest launch_request, const Hooks &hooks) {
    SDL_GLContext gl_context = nullptr;
    std::unique_ptr<sdl::FrameHost> frame_host;
    std::optional<AppLaunchRequest> pending_launch_request;
    app::LaunchRuntimeMetrics runtime_metrics{};
    SessionResult session_result{};
    bool window_notified = false;
    bool active = false;
    bool finished = false;

    const auto cleanup_launch = [&](const app::AppSessionStopReason reason) {
        if (hooks.on_stop)
            hooks.on_stop();
        update_mouse_input(emuenv, nullptr);
        update_keyboard(emuenv, nullptr);
        session.stop(reason);
        if (frame_host && gl_context)
            frame_host->destroy_render_context();
        reset_session_audio(emuenv);
        active = false;
        frame_host.reset();
    };

    const auto fail_launch = [&]() {
        session_result.exit_code = -1;
        cleanup_launch(app::AppSessionStopReason::LaunchFailure);
        finished = true;
    };

    std::function<void(AppLaunchRequest)> start_launch;
    std::function<void()> finish_current_launch;
    start_launch = [&](AppLaunchRequest request) {
        pending_launch_request.reset();
        runtime_metrics = {};
        LOG_INFO("Booting game '{}'", request.app_path);

        refresh_controllers(emuenv.ctrl, emuenv);
        if (hooks.on_init)
            hooks.on_init();

        if (!session.begin_launch(request, request.reason != AppLaunchReason::LoadExec)) {
            LOG_ERROR("Could not find app '{}' in apps list.", request.app_path);
            fail_launch();
            return;
        }
        if (hooks.on_launch)
            hooks.on_launch(request.app_path);
        sdl::set_loading_window_title(&window, emuenv);
        if (hooks.on_window && !window_notified) {
            hooks.on_window(&window);
            window_notified = true;
        }
        frame_host = std::make_unique<sdl::FrameHost>(&window, &gl_context);

        if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
            gl_context = SDL_GL_CreateContext(&window);
            if (!gl_context) {
                LOG_ERROR("Failed to create OpenGL context: {}", SDL_GetError());
                fail_launch();
                return;
            }
            if (!frame_host->make_current()) {
                LOG_ERROR("Failed to make OpenGL context current: {}", SDL_GetError());
                fail_launch();
                return;
            }
            if (!frame_host->set_vsync(emuenv.cfg.current_config.v_sync))
                LOG_WARN("Failed to set OpenGL swap interval: {}", SDL_GetError());
        }

        if (!session.initialize_renderer(*frame_host)) {
            LOG_ERROR("Failed to initialise renderer.");
            fail_launch();
            return;
        }
        if (!session.initialize_runtime()) {
            LOG_ERROR("Failed late initialisation.");
            fail_launch();
            return;
        }
        if (!session.load_and_run()) {
            LOG_ERROR("Failed to load or start the app session.");
            fail_launch();
            return;
        }

        const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
        if (user != emuenv.app.user_list.users.end() && user->second.system_music)
            bgm_player::switch_bgm_state(true);

        if (auto request = emuenv.take_app_launch_request())
            pending_launch_request = std::move(request);
        LOG_INFO("Game started: {} ({})", emuenv.current_app_title, request.app_path);
        active = true;
    };

    finish_current_launch = [&]() {
        if (pending_launch_request && pending_launch_request->reason == AppLaunchReason::ProcessExit) {
            session_result.exit_code = pending_launch_request->exit_code;
            session_result.app_exited = true;
            LOG_INFO("The app exited with code {}", session_result.exit_code);
            pending_launch_request.reset();
            cleanup_launch(app::AppSessionStopReason::UserRequest);
            finished = true;
        } else if (pending_launch_request) {
            auto next_launch = std::move(*pending_launch_request);
            pending_launch_request.reset();
            LOG_INFO("Relaunching in-process with self '{}'", next_launch.self_path);
            cleanup_launch(app::AppSessionStopReason::Relaunch);
            start_launch(std::move(next_launch));
        } else {
            cleanup_launch(app::AppSessionStopReason::UserRequest);
            finished = true;
        }
    };

    start_launch(std::move(launch_request));
    while (!finished) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_DROP_BEGIN:
            case SDL_EVENT_DROP_FILE:
            case SDL_EVENT_DROP_TEXT:
            case SDL_EVENT_DROP_COMPLETE:
            case SDL_EVENT_DROP_POSITION:
                sdl::discard_pending_drop_files();
                continue;
            default:
                break;
            }

            const auto shared_event = sdl::process_event(emuenv, event);
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && sdl::process_fullscreen_key(emuenv, &window, shared_event.physical_key))
                sdl::update_viewport(&window, emuenv.cfg.current_config.stretch_the_display_area,
                    emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load());
            if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
                sdl::update_viewport(&window, emuenv.cfg.current_config.stretch_the_display_area,
                    emuenv.cfg.current_config.fullscreen_hd_res_pixel_perfect, emuenv.display.fullscreen.load());
            if (!active || finished)
                continue;
            if (shared_event.outcome == sdl::EventOutcome::QuitRequested) {
                session_result.window_closed = true;
                continue;
            }
            switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                if (handle_ime_keydown(emuenv, hooks, event.key))
                    break;
                if (!event.key.repeat && SDL_GetKeyboardFocus() == &window && !is_any_ime_active(emuenv)
                    && event.key.scancode != SDL_SCANCODE_UNKNOWN) {
                    const auto &cfg = emuenv.cfg;
                    if (event.key.scancode == input::physical_key_to_sdl_scancode(cfg.keyboard_button_psbutton)
                        || event.key.scancode == input::physical_key_to_sdl_scancode(cfg.keyboard_button_psbutton_alt)) {
                        if (hooks.on_guide_button)
                            hooks.on_guide_button();
                        else if (session.is_running())
                            session.set_pause_reason(app::AppSessionPauseReason::User, !session.is_paused());
                    }
                }
                break;
            case SDL_EVENT_TEXT_EDITING:
                handle_ime_text_editing(emuenv, hooks, event.edit.text);
                break;
            case SDL_EVENT_TEXT_INPUT:
                handle_ime_text_input(emuenv, hooks, event.text.text);
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_MOTION:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_CANCELED: {
                auto &mouse = emuenv.ctrl.overlay_mouse;
                mouse.x.store(event.tfinger.x * 960.f, std::memory_order_relaxed);
                mouse.y.store(event.tfinger.y * 544.f, std::memory_order_relaxed);
                mouse.pressed.store(event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION, std::memory_order_relaxed);
                break;
            }
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (event.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) {
                    if (hooks.on_guide_button)
                        hooks.on_guide_button();
                    else if (session.is_running())
                        session.set_pause_reason(app::AppSessionPauseReason::User, !session.is_paused());
                }
                break;
            default:
                break;
            }
        }

        if (hooks.on_update)
            hooks.on_update();

        if (active && !finished) {
            if (session_result.window_closed) {
                cleanup_launch(app::AppSessionStopReason::FrontendShutdown);
                finished = true;
            } else {
                update_mouse_input(emuenv, &window);
                update_keyboard(emuenv, &window);
                if (!pending_launch_request) {
                    if (auto request = emuenv.take_app_launch_request())
                        pending_launch_request = std::move(request);
                }
                if (pending_launch_request) {
                    finish_current_launch();
                } else {
                    if (!session.is_paused() && app::update_runtime_metrics(emuenv, runtime_metrics))
                        sdl::update_runtime_window_title(&window, emuenv);
                    if (session.stop_requested() || !session.is_running()) {
                        session.reset_stop_request();
                        cleanup_launch(app::AppSessionStopReason::UserRequest);
                        finished = true;
                    }
                }
            }
        }
        if (!finished)
            SDL_Delay(16);
    }
    return session_result;
}

} // namespace sdl_frontend
