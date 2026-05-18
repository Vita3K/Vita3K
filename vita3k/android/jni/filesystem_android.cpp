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

#include <SDL3/SDL_system.h>
#include <SDL3/SDL_timer.h>

#include <atomic>
#include <jni.h>
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
static fs::path dialog_result_path{};

extern "C" JNIEXPORT void JNICALL
Java_org_vita3k_emulator_Emulator_filedialogReturn(JNIEnv *env, jobject thiz, jstring result_path) {
    const char *result_ptr = env->GetStringUTFChars(result_path, nullptr);
    dialog_result_path = fs::path(result_ptr);
    env->ReleaseStringUTFChars(result_path, result_ptr);

    file_dialog_running.store(false, std::memory_order_release);
}

static void call_dialog_java_function(const char *name, bool need_write) {
    (void)need_write;

    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    jclass clazz(env->GetObjectClass(activity));
    jmethodID method_id = env->GetMethodID(clazz, name, "()V");

    file_dialog_running = true;
    env->CallVoidMethod(activity, method_id);

    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);

    while (file_dialog_running.load(std::memory_order_acquire))
        SDL_Delay(10);
}

namespace host::dialog::filesystem {

Result open_file(fs::path &resulting_path, const std::vector<FileFilter> &file_filters, const fs::path &default_path) {
    call_dialog_java_function("showFileDialog", false);

    if (dialog_result_path.empty())
        return Result::CANCEL;

    resulting_path = std::move(dialog_result_path);
    return Result::SUCCESS;
}

Result pick_folder(fs::path &resulting_path, const fs::path &default_path) {
    call_dialog_java_function("showFolderDialog", true);

    if (dialog_result_path.empty())
        return Result::CANCEL;

    resulting_path = std::move(dialog_result_path);
    return Result::SUCCESS;
}

std::string get_error() {
    return "";
}

} // namespace host::dialog::filesystem
