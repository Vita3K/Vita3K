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
#include <ime/keyboard.h>
#include <ime/state.h>
#include <util/string_utils.h>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_system.h>
#include <jni.h>

namespace ime {

static SDL_Window *s_window = nullptr;

namespace {

void clear_ime_state(JNIEnv *env, jobject activity, jclass clazz) {
    const jmethodID method_id = env->GetMethodID(clazz, "clearNativeImeState", "()V");
    if (method_id)
        env->CallVoidMethod(activity, method_id);
}

void push_ime_state(JNIEnv *env, jobject activity, jclass clazz, EmuEnvState &emuenv) {
    const bool dialog_active = is_ime_dialog_active(emuenv);
    const bool sce_ime_active = emuenv.ime.state;

    if (!sce_ime_active && !dialog_active) {
        clear_ime_state(env, activity, clazz);
        return;
    }

    std::u16string text;
    uint32_t preedit_start = 0;
    uint32_t preedit_length = 0;
    uint32_t caret_index = 0;
    bool multiline = false;
    std::string enter_label;
    {
        std::lock_guard<std::recursive_mutex> dialog_lock(emuenv.common_dialog.mutex);
        std::lock_guard<std::mutex> ime_lock(emuenv.ime.mutex);
        text = emuenv.ime.str;
        preedit_start = emuenv.ime.edit_text.preeditIndex;
        preedit_length = emuenv.ime.edit_text.preeditLength;
        caret_index = emuenv.ime.edit_text.caretIndex;
        multiline = dialog_active
            ? emuenv.common_dialog.ime.multiline
            : ((emuenv.ime.param.option & SCE_IME_OPTION_MULTILINE) != 0);
        enter_label = emuenv.ime.enter_label;
    }

    const jmethodID method_id = env->GetMethodID(
        clazz,
        "updateNativeImeState",
        "(ZZLjava/lang/String;IIIZLjava/lang/String;)V");
    if (!method_id)
        return;

    const std::string utf8_text = string_utils::utf16_to_utf8(text);
    jstring text_value = env->NewStringUTF(utf8_text.c_str());
    jstring enter_label_value = env->NewStringUTF(enter_label.c_str());
    env->CallVoidMethod(activity,
        method_id,
        static_cast<jboolean>(sce_ime_active),
        static_cast<jboolean>(dialog_active),
        text_value,
        static_cast<jint>(preedit_start),
        static_cast<jint>(preedit_length),
        static_cast<jint>(caret_index),
        static_cast<jboolean>(multiline),
        enter_label_value);
    env->DeleteLocalRef(text_value);
    env->DeleteLocalRef(enter_label_value);
}

} // namespace

void set_sdl_window(SDL_Window *window) {
    s_window = window;
}

void set_keyboard_active(bool active) {
    if (s_window) {
        if (active)
            SDL_StartTextInput(s_window);
        else
            SDL_StopTextInput(s_window);
    }

    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    jclass clazz = env->GetObjectClass(activity);
    jmethodID method_id = env->GetMethodID(clazz, "setKeyboardActive", "(Z)V");
    if (method_id)
        env->CallVoidMethod(activity, method_id, static_cast<jboolean>(active));

    auto *emuenv = get_emuenv();
    if (emuenv)
        push_ime_state(env, activity, clazz, *emuenv);
    else
        clear_ime_state(env, activity, clazz);

    env->DeleteLocalRef(clazz);
    env->DeleteLocalRef(activity);
}

void notify_ime_state_changed() {
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity)
        return;

    jclass clazz = env->GetObjectClass(activity);
    auto *emuenv = get_emuenv();
    if (clazz) {
        if (emuenv)
            push_ime_state(env, activity, clazz, *emuenv);
        else
            clear_ime_state(env, activity, clazz);
        env->DeleteLocalRef(clazz);
    }
    env->DeleteLocalRef(activity);
}

} // namespace ime
