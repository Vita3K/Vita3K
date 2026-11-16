// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "android_state.h"

#include <app/functions.h>
#include <config/state.h>
#include <ime/functions.h>
#include <ime/keyboard.h>
#include <io/state.h>
#include <motion/functions.h>
#include <renderer/functions.h>

#include <SDL3/SDL_events.h>
#include <jni.h>

#include <algorithm>
#include <sstream>

namespace {

app::AppSessionPauseReason to_pause_reason(const jint reason_mask) {
    switch (static_cast<uint32_t>(reason_mask)) {
    case static_cast<uint32_t>(app::AppSessionPauseReason::User):
        return app::AppSessionPauseReason::User;
    case static_cast<uint32_t>(app::AppSessionPauseReason::Menu):
        return app::AppSessionPauseReason::Menu;
    case static_cast<uint32_t>(app::AppSessionPauseReason::Background):
        return app::AppSessionPauseReason::Background;
    default:
        return app::AppSessionPauseReason::None;
    }
}

std::vector<std::string> read_string_array(JNIEnv *env, jobjectArray array) {
    std::vector<std::string> values;
    if (!array)
        return values;

    const jsize count = env->GetArrayLength(array);
    values.reserve(static_cast<size_t>(std::max<jsize>(count, 0)));
    for (jsize index = 0; index < count; ++index) {
        auto *value = static_cast<jstring>(env->GetObjectArrayElement(array, index));
        if (!value) {
            values.emplace_back();
            continue;
        }

        values.push_back(jstring_to_string(env, value));
        env->DeleteLocalRef(value);
    }

    return values;
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_setInputIntercepted(JNIEnv *, jclass, jboolean intercepted) {
    auto *controller = get_app_session_controller();
    if (!controller || !controller->is_running())
        return JNI_FALSE;

    return controller->set_input_intercepted(intercepted == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_setPauseReasonEnabled(JNIEnv *, jclass, jint reason_mask, jboolean enabled) {
    auto *controller = get_app_session_controller();
    const auto reason = to_pause_reason(reason_mask);
    if (!controller || reason == app::AppSessionPauseReason::None || !controller->is_running())
        return JNI_FALSE;

    return controller->set_pause_reason(reason, enabled == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_isAppRunning(JNIEnv *, jclass) {
    auto *controller = get_app_session_controller();
    return controller && controller->has_active_session() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_vita3k_emulator_Emulator_setNativeDisplayRotation(JNIEnv *, jobject, jint rotation) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return;

    set_display_rotation(emuenv->motion, rotation);
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_requestAppQuit(JNIEnv *, jclass) {
    auto *controller = get_app_session_controller();
    if (!controller || !controller->has_active_session())
        return JNI_FALSE;

    SDL_Event quit_event{};
    quit_event.type = SDL_EVENT_QUIT;
    return SDL_PushEvent(&quit_event) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_requestAppRelaunch(JNIEnv *env, jclass, jstring title_id_str,
    jstring self_path_str, jobjectArray args_array, jboolean load_exec_reason) {
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();
    if (!emuenv || !controller || !controller->has_active_session())
        return JNI_FALSE;

    AppLaunchRequest launch_request{
        .app_path = title_id_str ? jstring_to_string(env, title_id_str) : emuenv->io.app_path,
        .self_path = self_path_str ? jstring_to_string(env, self_path_str) : std::string(),
        .argv = read_string_array(env, args_array),
        .reason = load_exec_reason == JNI_TRUE ? AppLaunchReason::LoadExec : AppLaunchReason::User
    };

    if (launch_request.app_path.empty())
        launch_request.app_path = emuenv->io.app_path;

    app::request_in_process_launch(*emuenv, std::move(launch_request));

    SDL_Event quit_event{};
    quit_event.type = SDL_EVENT_QUIT;
    return SDL_PushEvent(&quit_event) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_isAppPaused(JNIEnv *, jclass) {
    auto *controller = get_app_session_controller();
    if (!controller || !controller->is_running())
        return JNI_FALSE;

    return controller->is_paused() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_getRuntimeDiagnostics(JNIEnv *env, jclass) {
    std::ostringstream diagnostics;
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();

    diagnostics << "initialized=" << (emuenv ? "true" : "false") << '\n';
    diagnostics << "app_running=" << (controller && controller->has_active_session() ? "true" : "false") << '\n';
    diagnostics << "app_paused=" << (controller && controller->is_running() && controller->is_paused() ? "true" : "false") << '\n';

    if (!emuenv) {
        diagnostics << "backend=unavailable\n";
        diagnostics << "gpu=unavailable\n";
        return env->NewStringUTF(diagnostics.str().c_str());
    }

    diagnostics << "title=" << (controller ? controller->get_active_app_title() : std::string()) << '\n';
    diagnostics << "backend=" << emuenv->cfg.current_config.backend_renderer << '\n';
    diagnostics << "custom_driver=" << emuenv->cfg.current_config.custom_driver_name << '\n';

    const auto *vulkan_info = emuenv->vulkan_device_info.get();
    if (!vulkan_info || vulkan_info->gpu_names.size() <= 1) {
        diagnostics << "gpu=unavailable\n";
        diagnostics << "custom_driver_requested=false\n";
        diagnostics << "custom_driver_loaded=false\n";
    } else {
        diagnostics << "gpu=" << vulkan_info->gpu_names[1] << '\n';
        diagnostics << "custom_driver_requested=" << (vulkan_info->custom_driver_requested ? "true" : "false") << '\n';
        diagnostics << "custom_driver_loaded=" << (vulkan_info->custom_driver_loaded ? "true" : "false") << '\n';
        diagnostics << "mapping_masks=";
        for (size_t index = 0; index < vulkan_info->mapping_method_masks.size(); ++index) {
            if (index != 0)
                diagnostics << ',';
            diagnostics << vulkan_info->mapping_method_masks[index];
        }
        diagnostics << '\n';
    }

    const std::string result = diagnostics.str();
    return env->NewStringUTF(result.c_str());
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_getRunningAppTitle(JNIEnv *env, jclass) {
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();
    if (!emuenv || !controller || !controller->is_running())
        return env->NewStringUTF("");

    return env->NewStringUTF(emuenv->current_app_title.c_str());
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_isImeActive(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();
    if (!emuenv || !controller || !controller->is_running())
        return JNI_FALSE;

    return is_any_ime_active(*emuenv) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_submitIme(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();
    if (!emuenv || !controller || !controller->is_running())
        return JNI_FALSE;

    const bool submitted = submit_current_ime(*emuenv);
    if (submitted)
        ime::notify_ime_state_changed();
    return submitted ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_dismissIme(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();
    if (!emuenv || !controller || !controller->is_running())
        return JNI_FALSE;

    const bool dismissed = dismiss_current_ime(*emuenv);
    if (dismissed)
        ime::notify_ime_state_changed();
    return dismissed ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
