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

#include <app/functions.h>
#include <config/state.h>
#include <ctrl/functions.h>
#include <gui-imgui/functions.h>
#include <ime/keyboard.h>
#include <motion/functions.h>
#include <sdl-frontend/session.h>
#include <util/log.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdlib>
#include <regex>
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
        AppLaunchRequest launch_request{ .app_path = title_id };
        for (int i = 0; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg != "--self" && arg != "--app-args")
                continue;
            if (i + 1 >= argc) {
                LOG_ERROR("Missing value for {}", arg);
                return -1;
            }

            const std::string value = argv[++i];
            if (arg == "--self") {
                launch_request.self_path = value;
            } else if (!value.empty()) {
                const std::regex separator(",\\s+");
                launch_request.argv = std::vector<std::string>(
                    std::sregex_token_iterator(value.begin(), value.end(), separator, -1),
                    std::sregex_token_iterator());
            }
        }
        if (title_id.empty() && (!launch_request.self_path.empty() || !launch_request.argv.empty())) {
            LOG_ERROR("An app title ID is required with --self or --app-args");
            return -1;
        }
        if (!title_id.empty()) {
            emuenv->cfg.run_app_path = title_id;
            app::set_current_config(*emuenv, title_id);
        }

        if (!title_id.empty())
            emuenv->post_app_launch_request(std::move(launch_request));
        if (gui::run_frontend(*emuenv) == RendererInitFailed)
            return -1;
        // The process must not survive the frontend: static and global native state would leak into the next launch.
        // Exiting with 0 once everything is cleaned up avoids Android reporting a crash.
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
