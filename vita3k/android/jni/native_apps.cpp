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
#include <compat/functions.h>
#include <config/settings.h>
#include <io/state.h>
#include <util/log.h>

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_vita3k_emulator_NativeLib_getAppListDetailed(JNIEnv *env, jclass) {
    jclass app_info_class = env->FindClass("org/vita3k/emulator/data/NativeAppInfo");
    if (!app_info_class)
        return nullptr;

    jmethodID ctor = env->GetMethodID(app_info_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZIJJ)V");
    if (!ctor)
        return env->NewObjectArray(0, app_info_class, nullptr);

    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewObjectArray(0, app_info_class, nullptr);

    const auto apps = app::get_apps(*emuenv);
    const auto app_times = app::get_user_app_times(*emuenv);
    jobjectArray result = env->NewObjectArray(static_cast<jsize>(apps.size()), app_info_class, nullptr);

    for (jsize i = 0; i < static_cast<jsize>(apps.size()); ++i) {
        const auto &app = apps[static_cast<size_t>(i)];
        const int compat_state = static_cast<int>(compat::get_app_compat(emuenv->compat, app.title_id));
        const bool has_custom_config = config::has_custom_config(emuenv->config_path, app.path);

        int64_t last_played = 0;
        int64_t playtime = 0;
        const auto time_it = app_times.find(app.path);
        if (time_it != app_times.end()) {
            last_played = static_cast<int64_t>(time_it->second.last_time_used);
            playtime = time_it->second.time_used;
        }

        const fs::path icon_path = app.icon_path;
        const std::string icon_full = app.icon_path.empty()
            ? std::string()
            : (icon_path.is_absolute() ? icon_path.generic_string() : (emuenv->vita_fs_path / icon_path).generic_string());

        jstring title_id = env->NewStringUTF(app.title_id.c_str());
        jstring title = env->NewStringUTF(app.title.c_str());
        jstring category = env->NewStringUTF(app.category.c_str());
        jstring app_ver = env->NewStringUTF(app.app_ver.c_str());
        jstring icon_path_str = env->NewStringUTF(icon_full.c_str());

        jobject app_info = env->NewObject(app_info_class, ctor,
            title_id,
            title,
            category,
            app_ver,
            icon_path_str,
            static_cast<jboolean>(has_custom_config),
            static_cast<jint>(compat_state),
            static_cast<jlong>(last_played),
            static_cast<jlong>(playtime));

        env->SetObjectArrayElement(result, i, app_info);
        env->DeleteLocalRef(app_info);
        env->DeleteLocalRef(title_id);
        env->DeleteLocalRef(title);
        env->DeleteLocalRef(category);
        env->DeleteLocalRef(app_ver);
        env->DeleteLocalRef(icon_path_str);
    }

    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_performAppAction(JNIEnv *env, jclass, jstring title_id_str, jint action_bit) {
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();
    if (!emuenv)
        return JNI_FALSE;

    const std::string title_id = jstring_to_string(env, title_id_str);

    switch (static_cast<uint32_t>(action_bit)) {
    case ACTION_DELETE_APPLICATION: {
        if (controller && controller->has_active_session())
            return JNI_FALSE;
        const auto app = find_app_by_title_id(title_id);
        if (!app.has_value())
            return JNI_FALSE;
        app::delete_app(*emuenv, app->path);
        return JNI_TRUE;
    }
    case ACTION_DELETE_SAVE_DATA: {
        const auto app = find_app_by_title_id(title_id);
        if (!app.has_value())
            return JNI_FALSE;
        return remove_path_if_exists(emuenv->vita_fs_path / "ux0/user" / emuenv->io.user_id / "savedata" / app->savedata)
            ? JNI_TRUE
            : JNI_FALSE;
    }
    case ACTION_DELETE_PATCH: {
        bool removed = false;
        removed |= remove_path_if_exists(emuenv->vita_fs_path / "ux0/patch" / title_id);
        removed |= remove_path_if_exists(emuenv->shared_path / "patch" / title_id);
        return removed ? JNI_TRUE : JNI_FALSE;
    }
    case ACTION_DELETE_DLC: {
        const auto app = find_app_by_title_id(title_id);
        if (!app.has_value())
            return JNI_FALSE;
        return remove_path_if_exists(emuenv->vita_fs_path / "ux0/addcont" / app->addcont)
            ? JNI_TRUE
            : JNI_FALSE;
    }
    case ACTION_DELETE_LICENSE:
        return remove_path_if_exists(emuenv->vita_fs_path / "ux0/license" / title_id)
            ? JNI_TRUE
            : JNI_FALSE;
    case ACTION_DELETE_SHADER_CACHE:
        return remove_path_if_exists(emuenv->cache_path / "shaders" / title_id)
            ? JNI_TRUE
            : JNI_FALSE;
    case ACTION_DELETE_SHADER_LOG: {
        bool removed = false;
        removed |= remove_path_if_exists(emuenv->cache_path / "shaderlog" / title_id);
        removed |= remove_path_if_exists(emuenv->log_path / "shaderlog" / title_id);
        return removed ? JNI_TRUE : JNI_FALSE;
    }
    case ACTION_DELETE_EXPORT_TEXTURES:
        return remove_path_if_exists(emuenv->shared_path / "textures/export" / title_id)
            ? JNI_TRUE
            : JNI_FALSE;
    case ACTION_DELETE_IMPORT_TEXTURES:
        return remove_path_if_exists(emuenv->shared_path / "textures/import" / title_id)
            ? JNI_TRUE
            : JNI_FALSE;
    case ACTION_RESET_LAST_PLAYED: {
        const auto app = find_app_by_title_id(title_id);
        if (!app.has_value())
            return JNI_FALSE;
        app::reset_last_time_app_used(*emuenv, app->path);
        return JNI_TRUE;
    }
    default:
        return JNI_FALSE;
    }
}

JNIEXPORT jint JNICALL
Java_org_vita3k_emulator_NativeLib_getAppActionAvailabilityMask(JNIEnv *env, jclass, jstring title_id_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return static_cast<jint>(0);

    const std::string title_id = jstring_to_string(env, title_id_str);
    return static_cast<jint>(get_app_action_availability_mask(title_id));
}

JNIEXPORT jlong JNICALL
Java_org_vita3k_emulator_NativeLib_getAppInstallSize(JNIEnv *env, jclass, jstring title_id_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return 0L;

    const std::string title_id = jstring_to_string(env, title_id_str);
    const auto app = find_app_by_title_id(title_id);
    if (!app.has_value())
        return 0L;

    const auto app_path = emuenv->vita_fs_path / "ux0/app" / app->path;
    if (!fs::exists(app_path))
        return 0L;

    try {
        uintmax_t size = 0;
        for (const auto &entry : fs::recursive_directory_iterator(app_path, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file())
                size += fs::file_size(entry.path());
        }
        return static_cast<jlong>(size);
    } catch (const std::exception &error) {
        LOG_WARN("Failed to compute install size for {}: {}", title_id, error.what());
        return 0L;
    }
}

} // extern "C"
