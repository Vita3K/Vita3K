// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <SDL3/SDL_system.h>
#include <SDL3/SDL_timer.h>

#include <atomic>
#include <jni.h>
#include <mutex>
#include <string>
#include <vector>

#include <util/fs.h>

namespace host::dialog::filesystem {

enum Result {
    ERROR,
    SUCCESS,
    CANCEL,
};

struct FileFilter {
    std::string display_name = "";
    std::vector<std::string> file_extensions = {};
};

Result open_file(fs::path &resulting_path, const std::vector<FileFilter> &file_filters = {}, const fs::path &default_path = "");
Result pick_folder(fs::path &resulting_path, const fs::path &default_path = "");
std::string get_error();

} // namespace host::dialog::filesystem

static std::atomic<bool> file_dialog_running = false;
static std::mutex dialog_result_mutex;
static fs::path dialog_result_path{};

extern "C" JNIEXPORT void JNICALL
Java_org_vita3k_emulator_Emulator_filedialogReturn(JNIEnv *env, jobject thiz, jstring result_path) {
    {
        std::lock_guard lock(dialog_result_mutex);
        dialog_result_path = fs::path(jstring_to_string(env, result_path));
    }

    file_dialog_running.store(false, std::memory_order_release);
}

static bool call_dialog_java_function(const char *name, bool need_write) {
    (void)need_write;

    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity)
        return false;

    jclass clazz(env->GetObjectClass(activity));
    if (!clazz)
        return false;

    jmethodID method_id = env->GetMethodID(clazz, name, "()V");
    if (!method_id) {
        env->ExceptionClear();
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(clazz);
        return false;
    }

    {
        std::lock_guard lock(dialog_result_mutex);
        dialog_result_path.clear();
    }
    file_dialog_running.store(true, std::memory_order_release);
    env->CallVoidMethod(activity, method_id);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        file_dialog_running.store(false, std::memory_order_release);
    }

    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);

    while (file_dialog_running.load(std::memory_order_acquire))
        SDL_Delay(10);

    return true;
}

namespace host::dialog::filesystem {

Result open_file(fs::path &resulting_path, const std::vector<FileFilter> &file_filters, const fs::path &default_path) {
    if (!call_dialog_java_function("showFileDialog", false))
        return Result::ERROR;

    std::lock_guard lock(dialog_result_mutex);
    if (dialog_result_path.empty())
        return Result::CANCEL;

    resulting_path = std::move(dialog_result_path);
    return Result::SUCCESS;
}

Result pick_folder(fs::path &resulting_path, const fs::path &default_path) {
    if (!call_dialog_java_function("showFolderDialog", true))
        return Result::ERROR;

    std::lock_guard lock(dialog_result_mutex);
    if (dialog_result_path.empty())
        return Result::CANCEL;

    resulting_path = std::move(dialog_result_path);
    return Result::SUCCESS;
}

std::string get_error() {
    return "";
}

} // namespace host::dialog::filesystem
