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

#include "frame_host.h"

#include <sdl-frontend/session.h>

#include <app/functions.h>
#include <app/session_controller.h>
#include <audio/state.h>
#include <config/state.h>
#include <ctrl/functions.h>
#include <emuenv/state.h>
#include <ime/functions.h>
#include <motion/event_handler.h>
#include <renderer/state.h>
#include <touch/functions.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL3/SDL.h>

#include <cstring>
#include <mutex>
#include <optional>

namespace sdl_frontend {

namespace {

// SDL_Quit at the end of a session stops the audio device, so the next session opens a new one
void reset_session_audio(EmuEnvState &emuenv) {
    if (!emuenv.audio.adapter)
        return;

    LOG_DEBUG("Resetting audio backend '{}' after SDL session shutdown", emuenv.audio.audio_backend);
    emuenv.audio.adapter.reset();
    emuenv.audio.audio_backend.clear();
}

void notify_ime_changed(const Hooks &hooks) {
    if (hooks.on_ime_changed)
        hooks.on_ime_changed();
}

// Returns whether the key was for the active IME
bool handle_ime_keydown(EmuEnvState &emuenv, const Hooks &hooks, const SDL_KeyboardEvent &event) {
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

void handle_ime_text_editing(EmuEnvState &emuenv, const Hooks &hooks, const char *text) {
    if (!is_any_ime_active(emuenv))
        return;

    {
        const std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        ime_set_preedit(emuenv.ime, string_utils::utf8_to_utf16(text ? text : ""));
    }
    notify_ime_changed(hooks);
}

void handle_ime_text_input(EmuEnvState &emuenv, const Hooks &hooks, const char *text) {
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

// Sets the attributes of the OpenGL context the renderer needs. Returns false if SDL refused one.
bool set_gl_attributes() {
#ifdef __ANDROID__
    const bool set = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES)
        && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
        && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const bool set = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)
        && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4)
        && SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    if (!set || !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) || !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0))
        return false;
#ifndef NDEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    return true;
}

} // namespace

int run(EmuEnvState &emuenv, app::AppSessionController &session, AppLaunchRequest launch_request, const Hooks &hooks) {
    int exit_code = 0;
    bool relaunch_requested = false;

    do {
        relaunch_requested = false;

        SDL_Window *window = nullptr;
        SDL_GLContext gl_context = nullptr;
        FrameHost frame_host(nullptr, &gl_context);
        std::optional<AppLaunchRequest> pending_launch_request;

        const auto cleanup_launch = [&](const app::AppSessionStopReason reason) {
            if (hooks.on_stop)
                hooks.on_stop();
            session.stop(reason);
            reset_session_audio(emuenv);

            if (hooks.on_window)
                hooks.on_window(nullptr);
            if (window) {
                SDL_DestroyWindow(window);
                window = nullptr;
            }

            SDL_Quit();
        };

        LOG_INFO("Booting game '{}'", launch_request.app_path);

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_SENSOR | SDL_INIT_CAMERA)) {
            LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
            exit_code = -1;
            cleanup_launch(app::AppSessionStopReason::LaunchFailure);
            break;
        }

        refresh_controllers(emuenv.ctrl, emuenv);
        if (hooks.on_init)
            hooks.on_init();

        if (!session.begin_launch(launch_request, launch_request.reason != AppLaunchReason::LoadExec)) {
            LOG_ERROR("Could not find app '{}' in apps list.", launch_request.app_path);
            exit_code = -1;
            cleanup_launch(app::AppSessionStopReason::LaunchFailure);
            break;
        }
        if (hooks.on_launch)
            hooks.on_launch(launch_request.app_path);

        SDL_WindowFlags window_flags = 0;
#ifndef __ANDROID__
        window_flags |= SDL_WINDOW_RESIZABLE;
#endif
        if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
            window_flags |= SDL_WINDOW_OPENGL;
            if (!set_gl_attributes()) {
                LOG_ERROR("Failed to configure OpenGL context attributes: {}", SDL_GetError());
                exit_code = -1;
                cleanup_launch(app::AppSessionStopReason::LaunchFailure);
                break;
            }
        } else if (emuenv.backend_renderer == renderer::Backend::Vulkan) {
            window_flags |= SDL_WINDOW_VULKAN;
        }

        SDL_PropertiesID window_props = SDL_CreateProperties();
        if (!window_props) {
            LOG_ERROR("SDL_CreateProperties failed: {}", SDL_GetError());
            exit_code = -1;
            cleanup_launch(app::AppSessionStopReason::LaunchFailure);
            break;
        }

        SDL_SetStringProperty(window_props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Vita3K");
        SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 960);
        SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 544);
        SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, window_flags);
        if (emuenv.backend_renderer == renderer::Backend::OpenGL)
            SDL_SetBooleanProperty(window_props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);

        window = SDL_CreateWindowWithProperties(window_props);
        SDL_DestroyProperties(window_props);
        if (!window) {
            LOG_ERROR("SDL_CreateWindowWithProperties failed: {}", SDL_GetError());
            exit_code = -1;
            cleanup_launch(app::AppSessionStopReason::LaunchFailure);
            break;
        }
        if (hooks.on_window)
            hooks.on_window(window);
        frame_host = FrameHost(window, &gl_context);

        if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
            gl_context = SDL_GL_CreateContext(window);
            if (!gl_context) {
                LOG_ERROR("Failed to create OpenGL context: {}", SDL_GetError());
                exit_code = -1;
                cleanup_launch(app::AppSessionStopReason::LaunchFailure);
                break;
            }

            if (!SDL_GL_MakeCurrent(window, gl_context)) {
                LOG_ERROR("Failed to make OpenGL context current: {}", SDL_GetError());
                exit_code = -1;
                cleanup_launch(app::AppSessionStopReason::LaunchFailure);
                break;
            }

            if (!SDL_GL_SetSwapInterval(static_cast<int>(emuenv.cfg.current_config.v_sync)))
                LOG_WARN("Failed to set OpenGL swap interval: {}", SDL_GetError());
        }

        if (!session.initialize_renderer(frame_host)) {
            LOG_ERROR("Failed to initialise renderer.");
            exit_code = -1;
            cleanup_launch(app::AppSessionStopReason::LaunchFailure);
            break;
        }

        if (!session.initialize_runtime()) {
            LOG_ERROR("Failed late initialisation.");
            exit_code = -1;
            cleanup_launch(app::AppSessionStopReason::LaunchFailure);
            break;
        }

        if (!session.load_and_run()) {
            LOG_ERROR("Failed to load or start the app session.");
            exit_code = -1;
            cleanup_launch(app::AppSessionStopReason::LaunchFailure);
            break;
        }

        if (auto request = emuenv.take_app_launch_request())
            pending_launch_request = std::move(request);

        LOG_INFO("Game started: {} ({})", emuenv.current_app_title, launch_request.app_path);
        app::LaunchRuntimeMetrics runtime_metrics{};

        bool running = !pending_launch_request.has_value();
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    running = false;
                    break;

                case SDL_EVENT_KEY_DOWN:
                    handle_ime_keydown(emuenv, hooks, event.key);
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
                    handle_touch_event(emuenv.touch, event.tfinger);
                    auto &mouse = emuenv.ctrl.overlay_mouse;
                    mouse.x.store(event.tfinger.x * 960.f, std::memory_order_relaxed);
                    mouse.y.store(event.tfinger.y * 544.f, std::memory_order_relaxed);
                    mouse.pressed.store(event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION, std::memory_order_relaxed);
                    break;
                }

                case SDL_EVENT_GAMEPAD_ADDED:
                case SDL_EVENT_GAMEPAD_REMOVED:
                    refresh_controllers(emuenv.ctrl, emuenv);
                    break;

                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                    if (event.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE && hooks.on_guide_button)
                        hooks.on_guide_button();
                    break;

                case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
                case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
                case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
                    handle_touchpad_event(emuenv.touch, event.gtouchpad);
                    break;

                case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
                    handle_motion_event(emuenv, event.gsensor.sensor, event.gsensor);
                    break;
                case SDL_EVENT_SENSOR_UPDATE:
                    handle_motion_event(emuenv, SDL_GetSensorTypeForID(event.sensor.which), event.sensor);
                    break;

                default:
                    break;
                }
            }

            if (!pending_launch_request) {
                if (auto request = emuenv.take_app_launch_request()) {
                    pending_launch_request = std::move(request);
                    running = false;
                }
            }

            app::update_runtime_metrics(emuenv, runtime_metrics);

            if (!session.is_running())
                running = false;

            if (running)
                SDL_Delay(16);
        }

        LOG_INFO("Shutting down game");

        if (pending_launch_request) {
            launch_request = std::move(*pending_launch_request);
            relaunch_requested = true;
            LOG_INFO("Relaunching in-process with self '{}'", launch_request.self_path);
        }

        cleanup_launch(relaunch_requested
                ? app::AppSessionStopReason::Relaunch
                : app::AppSessionStopReason::UserRequest);
    } while (relaunch_requested);

    return exit_code;
}

} // namespace sdl_frontend
