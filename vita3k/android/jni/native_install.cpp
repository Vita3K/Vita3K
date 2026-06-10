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
#include "archive.h"

#include <app/functions.h>
#include <packages/functions.h>
#include <packages/license.h>
#include <packages/pkg.h>
#include <rif2zrif.h>
#include <util/log.h>

extern "C" {

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_installFirmware(JNIEnv *env, jclass, jstring path_str, jobject callback) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewStringUTF("");

    const std::string path = jstring_to_string(env, path_str);
    ScopedJniCallback progress_callback(env, callback, "onProgress", "(ILjava/lang/String;)V");

    auto progress_fn = [&progress_callback](uint32_t percent) {
        progress_callback.call_progress(static_cast<int>(percent), "Installing firmware...");
    };

    const std::string version = install_pup(emuenv->vita_fs_path, fs::path(path), progress_fn);
    if (!version.empty()) {
        LOG_INFO("Firmware {} installed successfully", version);
        app::scan_apps(*emuenv);
    } else {
        LOG_ERROR("Firmware installation failed for: {}", path);
    }

    return env->NewStringUTF(version.c_str());
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_installPkg(JNIEnv *env, jclass, jstring path_str, jstring zrif_str, jobject callback) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return JNI_FALSE;

    const std::string path = jstring_to_string(env, path_str);
    std::string zrif = jstring_to_string(env, zrif_str);
    ScopedJniCallback progress_callback(env, callback, "onProgress", "(ILjava/lang/String;)V");

    auto progress_fn = [&progress_callback](float percent) {
        progress_callback.call_progress(static_cast<int>(percent), "Installing package...");
    };

    const bool success = install_pkg(fs::path(path), *emuenv, zrif, progress_fn);
    if (success) {
        LOG_INFO("PKG installed successfully");
        app::scan_apps(*emuenv);
    } else {
        LOG_ERROR("PKG installation failed for: {} (zRIF {})", path, zrif.empty() ? "not provided" : "provided");
    }

    return success ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_installArchive(JNIEnv *env, jclass, jstring path_str, jobject callback, jboolean force_reinstall) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return JNI_FALSE;

    const std::string path = jstring_to_string(env, path_str);
    ScopedJniCallback progress_callback(env, callback, "onProgress", "(ILjava/lang/String;)V");

    auto progress_fn = [&progress_callback](ArchiveContents contents) {
        int percent = 0;
        if (contents.progress.has_value()) {
            percent = static_cast<int>(contents.progress.value());
        } else if (contents.count.has_value() && contents.current.has_value() && contents.count.value() > 0) {
            percent = static_cast<int>((static_cast<float>(contents.current.value()) / contents.count.value()) * 100.0f);
        }
        progress_callback.call_progress(percent, "Installing archive...");
    };

    const auto reinstall_fn = [force_reinstall](const std::string &, const std::string &) -> bool { return static_cast<bool>(force_reinstall); };
    const auto result = install_archive(*emuenv, fs::path(path), progress_fn, reinstall_fn);
    if (!result.empty())
        app::scan_apps(*emuenv);

    return result.empty() ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_copyLicense(JNIEnv *env, jclass, jstring path_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return JNI_FALSE;

    const std::string path = jstring_to_string(env, path_str);
    return copy_license(*emuenv, fs::path(path)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_createLicense(JNIEnv *env, jclass, jstring zrif_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return JNI_FALSE;

    const std::string zrif = jstring_to_string(env, zrif_str);
    return create_license(*emuenv, zrif) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_findPkgZrif(JNIEnv *env, jclass, jstring pkg_path_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewStringUTF("");

    const std::string path = jstring_to_string(env, pkg_path_str);
    const std::string zrif = find_pkg_zrif(fs::path(path), emuenv->vita_fs_path);
    return env->NewStringUTF(zrif.c_str());
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_convertRifToZrif(JNIEnv *env, jclass, jstring path_str) {
    const std::string path = jstring_to_string(env, path_str);
    fs::ifstream binfile(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!binfile.is_open()) {
        LOG_ERROR("Failed to open license from path: {}", path);
        return env->NewStringUTF("");
    }

    const std::string zrif = rif2zrif(binfile);
    return env->NewStringUTF(zrif.c_str());
}

} // extern "C"
