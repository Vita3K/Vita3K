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

#include "android_state.h"
#include "interface.h"

#include <app/functions.h>
#include <audio/state.h>
#include <config/settings.h>
#include <ctrl/functions.h>
#include <dialog/state.h>
#include <gui/functions.h>
#include <ime/functions.h>
#include <ime/keyboard.h>
#include <io/state.h>
#include <motion/functions.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <sdl-frontend/session.h>
#include <sdl-frontend/src/frame_host.h>
#include <util/log.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>

namespace {

// Calls a void method of the Android activity, with args after the JNI environment and activity
template <typename... Args>
void call_activity(const char *name, const char *signature, Args... args) {
    JNIEnv *jni_env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    if (!jni_env || !activity)
        return;

    jclass clazz = jni_env->GetObjectClass(activity);
    jmethodID method_id = jni_env->GetMethodID(clazz, name, signature);
    if (method_id)
        jni_env->CallVoidMethod(activity, method_id, args...);

    jni_env->DeleteLocalRef(clazz);
    jni_env->DeleteLocalRef(activity);
}

int get_display_rotation() {
    JNIEnv *jni_env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    jclass clazz = jni_env->GetObjectClass(activity);
    jmethodID method_id = jni_env->GetMethodID(clazz, "getNativeDisplayRotation", "()I");
    const int rotation = jni_env->CallIntMethod(activity, method_id);
    jni_env->DeleteLocalRef(clazz);
    jni_env->DeleteLocalRef(activity);
    return rotation;
}

} // namespace

extern "C" {

// SDL3 entry point — called by SDLActivity's native thread when the Emulator activity starts.
// argv is populated from Emulator.getArguments(), e.g. {"-r", "PCSE00000"}.
SDLMAIN_DECLSPEC int SDL_main(int argc, char *argv[]) {
    const bool imgui_frontend = std::any_of(argv, argv + argc, [](const char *arg) {
        return std::string(arg) == "--imgui";
    });
    std::string title_id;
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "-r" && i + 1 < argc) {
            title_id = argv[i + 1];
            break;
        }
    }

    if (title_id.empty() && !imgui_frontend) {
        LOG_ERROR("No title ID provided");
        return -1;
    }

    auto *emuenv = get_emuenv();
    if (!emuenv) {
        LOG_ERROR("Emulator not initialized");
        return -1;
    }

    if (imgui_frontend) {
        if (!title_id.empty()) {
            emuenv->cfg.run_app_path = title_id;
            app::set_current_config(*emuenv, title_id);
        }

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_SENSOR | SDL_INIT_CAMERA)) {
            LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
            SDL_Quit();
            return -1;
        }

        SDL_GLContext gl_context = nullptr;
        const auto shutdown_frontend = [&] {
            if (emuenv->renderer) {
                renderer::stop_render_thread(*emuenv->renderer);
                emuenv->renderer->cleanup();
                emuenv->renderer.reset();
            }
            emuenv->frame_host.reset();
            if (gl_context) {
                SDL_GL_DestroyContext(gl_context);
                gl_context = nullptr;
            }
            ime::set_sdl_window(nullptr);
            emuenv->window.reset();
            if (emuenv->audio.adapter) {
                emuenv->audio.adapter.reset();
                emuenv->audio.audio_backend.clear();
            }
            SDL_Quit();
        };

        SDL_WindowFlags window_flags = emuenv->backend_renderer == renderer::Backend::OpenGL ? SDL_WINDOW_OPENGL : SDL_WINDOW_VULKAN;
        if (emuenv->backend_renderer == renderer::Backend::OpenGL) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        }

        SDL_PropertiesID window_props = SDL_CreateProperties();
        if (!window_props) {
            SDL_Quit();
            return -1;
        }
        SDL_SetStringProperty(window_props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Vita3K");
        SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 960);
        SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 544);
        SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, window_flags);
        if (emuenv->backend_renderer == renderer::Backend::OpenGL)
            SDL_SetBooleanProperty(window_props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
        auto *window = SDL_CreateWindowWithProperties(window_props);
        SDL_DestroyProperties(window_props);
        if (!window) {
            LOG_ERROR("SDL_CreateWindowWithProperties failed: {}", SDL_GetError());
            SDL_Quit();
            return -1;
        }

        if (emuenv->backend_renderer == renderer::Backend::OpenGL) {
            gl_context = SDL_GL_CreateContext(window);
            if (!gl_context || !SDL_GL_MakeCurrent(window, gl_context)) {
                LOG_ERROR("Failed to create OpenGL ES context: {}", SDL_GetError());
                if (gl_context)
                    SDL_GL_DestroyContext(gl_context);
                SDL_DestroyWindow(window);
                SDL_Quit();
                return -1;
            }
            SDL_GL_SetSwapInterval(static_cast<int>(emuenv->cfg.current_config.v_sync));
        }

        emuenv->window = WindowPtr(window, SDL_DestroyWindow);
        emuenv->frame_host = std::make_unique<sdl_frontend::FrameHost>(window, &gl_context);
        ime::set_sdl_window(window);
        refresh_controllers(emuenv->ctrl, *emuenv);
        if (!renderer::init(*emuenv->frame_host, emuenv->renderer, emuenv->backend_renderer, emuenv->cfg, emuenv->get_root_paths())) {
            LOG_ERROR("Failed to initialize the ImGui frontend renderer.");
            shutdown_frontend();
            return -1;
        }
        app::apply_renderer_config(*emuenv);
        const int exit_code = static_cast<int>(gui::run_frontend(*emuenv));
        shutdown_frontend();
        // The process must not survive the frontend: static and global native state would leak into the next launch.
        // Exiting with 0 once everything is cleaned up avoids Android reporting a crash.
        (void)exit_code;
        exit(0);
    }

    auto *session_controller = get_app_session_controller();
    if (!session_controller) {
        LOG_ERROR("App session controller is unavailable");
        return -1;
    }

    const sdl_frontend::Hooks hooks{
        .on_init = [emuenv] { set_display_rotation(emuenv->motion, get_display_rotation()); },
        .on_window = [](SDL_Window *window) { ime::set_sdl_window(window); },
        .on_launch = [](const std::string &game_id) {
            JNIEnv *jni_env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
            if (!jni_env)
                return;
            jstring jgame_id = jni_env->NewStringUTF(game_id.c_str());
            call_activity("setCurrentGameId", "(Ljava/lang/String;)V", jgame_id);
            jni_env->DeleteLocalRef(jgame_id); },
        .on_guide_button = [] { call_activity("openPauseMenuFromController", "()V"); },
        .on_ime_changed = [] { ime::notify_ime_state_changed(); },
        .on_stop = [] { detach_overlay_virtual_controller(); },
    };

    return sdl_frontend::run(*emuenv, *session_controller, AppLaunchRequest{ .app_path = title_id }, hooks);
}

} // extern "C"
