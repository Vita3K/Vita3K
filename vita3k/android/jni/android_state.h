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

#pragma once

#include <app/functions.h>
#include <app/session_controller.h>
#include <emuenv/state.h>
#include <util/fs.h>

#include <jni.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

struct AndroidSessionState {
    std::unique_ptr<EmuEnvState> emuenv;
    std::unique_ptr<app::AppSessionController> app_session_controller;
    Root root_paths;
};

AndroidSessionState &android_session_state();
EmuEnvState *get_emuenv();
app::AppSessionController *get_app_session_controller();
Root &get_root_paths();

std::string jstring_to_string(JNIEnv *env, jstring str);
bool is_ime_dialog_active(const EmuEnvState &emuenv);
bool is_any_ime_active(const EmuEnvState &emuenv);
void finish_ime_dialog(EmuEnvState &emuenv);
void cancel_ime_dialog(EmuEnvState &emuenv);
bool submit_current_ime(EmuEnvState &emuenv);
bool dismiss_current_ime(EmuEnvState &emuenv);

class ScopedJniCallback {
public:
    ScopedJniCallback(JNIEnv *env, jobject callback_obj, const char *method_name, const char *method_sig);
    ~ScopedJniCallback();

    void call_progress(int percent, const char *status) const;

private:
    JNIEnv *env_ = nullptr;
    JavaVM *java_vm_ = nullptr;
    jobject callback_ = nullptr;
    jmethodID method_ = nullptr;
};

std::optional<app::AppEntry> find_app_by_title_id(const std::string &title_id);
bool remove_path_if_exists(const fs::path &path);
void attach_overlay_virtual_controller();
void detach_overlay_virtual_controller();

enum AppActionMask : uint32_t {
    ACTION_DELETE_APPLICATION = 1u << 0,
    ACTION_DELETE_SAVE_DATA = 1u << 1,
    ACTION_DELETE_PATCH = 1u << 2,
    ACTION_DELETE_DLC = 1u << 3,
    ACTION_DELETE_LICENSE = 1u << 4,
    ACTION_DELETE_SHADER_CACHE = 1u << 5,
    ACTION_DELETE_SHADER_LOG = 1u << 6,
    ACTION_DELETE_EXPORT_TEXTURES = 1u << 7,
    ACTION_DELETE_IMPORT_TEXTURES = 1u << 8,
    ACTION_RESET_LAST_PLAYED = 1u << 9,
};

uint32_t get_app_action_availability_mask(const std::string &title_id);
