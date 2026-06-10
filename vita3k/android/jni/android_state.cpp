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

#include <dialog/state.h>
#include <ime/state.h>
#include <io/state.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>

namespace {

AndroidSessionState session_state;

} // namespace

AndroidSessionState &android_session_state() {
    return session_state;
}

EmuEnvState *get_emuenv() {
    return android_session_state().emuenv.get();
}

app::AppSessionController *get_app_session_controller() {
    return android_session_state().app_session_controller.get();
}

Root &get_root_paths() {
    return android_session_state().root_paths;
}

std::string jstring_to_string(JNIEnv *env, jstring str) {
    const char *utf = env->GetStringUTFChars(str, nullptr);
    std::string result(utf);
    env->ReleaseStringUTFChars(str, utf);
    return result;
}

bool is_ime_dialog_active(const EmuEnvState &emuenv) {
    return emuenv.common_dialog.type == IME_DIALOG
        && emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING;
}

bool is_any_ime_active(const EmuEnvState &emuenv) {
    return emuenv.ime.state || is_ime_dialog_active(emuenv);
}

void finish_ime_dialog(EmuEnvState &emuenv) {
    auto &dialog = emuenv.common_dialog;
    auto &ime = emuenv.ime;

    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
    std::lock_guard<std::mutex> ime_lock(ime.mutex);

    const size_t copy_len = std::min(static_cast<size_t>(ime.str.length()),
        static_cast<size_t>(dialog.ime.max_length));
    if (dialog.ime.result) {
        std::memcpy(dialog.ime.result, ime.str.c_str(), copy_len * sizeof(uint16_t));
        dialog.ime.result[copy_len] = 0;
    }

    const std::string utf8 = string_utils::utf16_to_utf8(ime.str);
    std::snprintf(dialog.ime.text, sizeof(dialog.ime.text), "%s", utf8.c_str());
    dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
}

void cancel_ime_dialog(EmuEnvState &emuenv) {
    auto &dialog = emuenv.common_dialog;
    if (!dialog.ime.cancelable)
        return;

    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
    dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}

bool submit_current_ime(EmuEnvState &emuenv) {
    if (!is_any_ime_active(emuenv))
        return false;

    if (is_ime_dialog_active(emuenv)) {
        finish_ime_dialog(emuenv);
    } else {
        std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        emuenv.ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
    }

    return true;
}

bool dismiss_current_ime(EmuEnvState &emuenv) {
    if (!is_any_ime_active(emuenv))
        return false;

    if (is_ime_dialog_active(emuenv)) {
        cancel_ime_dialog(emuenv);
        if (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_FINISHED)
            return false;
    } else {
        std::lock_guard<std::mutex> lock(emuenv.ime.mutex);
        emuenv.ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
    }

    return true;
}

ScopedJniCallback::ScopedJniCallback(JNIEnv *env, jobject callback_obj, const char *method_name, const char *method_sig)
    : env_(env) {
    env->GetJavaVM(&java_vm_);
    callback_ = env->NewGlobalRef(callback_obj);
    jclass callback_class = env->GetObjectClass(callback_obj);
    method_ = env->GetMethodID(callback_class, method_name, method_sig);
    env->DeleteLocalRef(callback_class);
}

ScopedJniCallback::~ScopedJniCallback() {
    if (!callback_ || !java_vm_)
        return;

    JNIEnv *env = nullptr;
    bool attached = false;
    const jint result = java_vm_->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        attached = java_vm_->AttachCurrentThread(&env, nullptr) == JNI_OK;
        if (!env)
            return;
    } else if (result != JNI_OK) {
        return;
    }

    env->DeleteGlobalRef(callback_);
    callback_ = nullptr;

    if (attached)
        java_vm_->DetachCurrentThread();
}

void ScopedJniCallback::call_progress(int percent, const char *status) const {
    if (!callback_ || !method_ || !env_)
        return;

    jstring jstatus = env_->NewStringUTF(status);
    env_->CallVoidMethod(callback_, method_, static_cast<jint>(percent), jstatus);
    env_->DeleteLocalRef(jstatus);

    if (env_->ExceptionCheck()) {
        env_->ExceptionDescribe();
        env_->ExceptionClear();
    }
}

std::optional<app::AppEntry> find_app_by_title_id(const std::string &title_id) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return std::nullopt;

    const auto apps = app::get_apps(*emuenv);
    const auto it = std::find_if(apps.begin(), apps.end(), [&](const app::AppEntry &app) {
        return app.title_id == title_id;
    });
    if (it == apps.end())
        return std::nullopt;

    return *it;
}

bool remove_path_if_exists(const fs::path &path) {
    if (!fs::exists(path))
        return false;

    try {
        fs::remove_all(path);
        return true;
    } catch (const std::exception &error) {
        LOG_WARN("Failed to remove '{}': {}", path.string(), error.what());
        return false;
    }
}

uint32_t get_app_action_availability_mask(const std::string &title_id) {
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();
    if (!emuenv)
        return 0;

    uint32_t mask = 0;
    const auto app = find_app_by_title_id(title_id);
    const bool has_app = app.has_value();

    if (has_app && (!controller || !controller->has_active_session()))
        mask |= ACTION_DELETE_APPLICATION;

    if (has_app && !app->savedata.empty() && fs::exists(emuenv->vita_fs_path / "ux0/user" / emuenv->io.user_id / "savedata" / app->savedata))
        mask |= ACTION_DELETE_SAVE_DATA;

    if (fs::exists(emuenv->vita_fs_path / "ux0/patch" / title_id)
        || fs::exists(emuenv->shared_path / "patch" / title_id))
        mask |= ACTION_DELETE_PATCH;

    if (has_app && !app->addcont.empty() && fs::exists(emuenv->vita_fs_path / "ux0/addcont" / app->addcont))
        mask |= ACTION_DELETE_DLC;

    if (fs::exists(emuenv->vita_fs_path / "ux0/license" / title_id))
        mask |= ACTION_DELETE_LICENSE;

    if (fs::exists(emuenv->cache_path / "shaders" / title_id))
        mask |= ACTION_DELETE_SHADER_CACHE;

    if (fs::exists(emuenv->cache_path / "shaderlog" / title_id)
        || fs::exists(emuenv->log_path / "shaderlog" / title_id))
        mask |= ACTION_DELETE_SHADER_LOG;

    if (fs::exists(emuenv->shared_path / "textures/export" / title_id))
        mask |= ACTION_DELETE_EXPORT_TEXTURES;

    if (fs::exists(emuenv->shared_path / "textures/import" / title_id))
        mask |= ACTION_DELETE_IMPORT_TEXTURES;

    if (has_app)
        mask |= ACTION_RESET_LAST_PLAYED;

    return mask;
}
