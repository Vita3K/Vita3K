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
#include <compat/functions.h>
#include <compat/state.h>
#include <config/functions.h>
#include <config/settings.h>
#include <config/version.h>
#include <modules/module_parent.h>
#include <renderer/functions.h>
#include <util/log.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <filesystem>
#include <span>
#include <vector>

namespace {

constexpr jint k_preinstalled_firmware_bit = 1 << 0;
constexpr jint k_main_firmware_bit = 1 << 1;
constexpr jint k_font_firmware_bit = 1 << 2;

std::string format_app_version() {
    std::string version = app_version;
    version += "-";
    version += std::to_string(app_number);
    version += "-";
    version += app_hash;
    return version;
}

bool initialize_session(const fs::path &storage_path, Root &root_paths, std::unique_ptr<EmuEnvState> &emuenv) {
    try {
        const fs::path vita_path = storage_path / "vita" / "";

        root_paths.set_static_assets_path({});
        root_paths.set_vita_fs_path(vita_path);
        root_paths.set_log_path(storage_path);
        root_paths.set_config_path(storage_path);
        root_paths.set_shared_path(storage_path);
        root_paths.set_cache_path(storage_path / "cache" / "");
        root_paths.set_patch_path(storage_path / "patch" / "");

        if (!fs::exists(root_paths.get_vita_fs_path()))
            fs::create_directories(root_paths.get_vita_fs_path());

        fs::create_directories(root_paths.get_config_path());
        fs::create_directories(root_paths.get_cache_path());
        fs::create_directories(root_paths.get_log_path() / "shaderlog");
        fs::create_directories(root_paths.get_log_path() / "texturelog");
        fs::create_directories(root_paths.get_patch_path());
        fs::create_directories(root_paths.get_shared_path() / "textures");

        if (logging::init(root_paths, true) != Success)
            return false;

        LOG_INFO("{}", window_title);

        emuenv = std::make_unique<EmuEnvState>();

        Config cfg{};
        char arg0[] = "vita3k";
        char *argv[] = { arg0, nullptr };
        if (config::init_config(cfg, 1, argv, root_paths, false) != Success) {
            LOG_ERROR("Failed to initialise config.");
            emuenv.reset();
            return false;
        }

        fs::create_directories(cfg.get_vita_fs_path());

        if (!app::init(*emuenv, cfg, root_paths)) {
            LOG_ERROR("Failed to initialise emulated environment.");
            emuenv.reset();
            return false;
        }

        emuenv->vulkan_device_info = std::make_unique<renderer::VulkanDeviceInfo>(renderer::enumerate_vulkan_devices());

        if (emuenv->cfg.controller_binds.empty() || emuenv->cfg.controller_binds.size() != 15
            || emuenv->cfg.controller_axis_binds.empty() || emuenv->cfg.controller_axis_binds.size() != 6) {
            app::reset_controller_binding(*emuenv);
        }

        init_libraries(*emuenv);

        if (!app::init_apps_list(*emuenv))
            LOG_ERROR("Failed to initialise apps list.");

        app::load_users(*emuenv);
        if (!app::ensure_current_user(*emuenv)) {
            LOG_ERROR("Failed to initialize active user.");
            return false;
        }
        compat::load_from_disk(emuenv->compat, std::filesystem::path(emuenv->cache_path.string()));
        return true;
    } catch (const std::exception &error) {
        LOG_ERROR("Failed to initialize Android storage path '{}': {}", storage_path, error.what());
        emuenv.reset();
        return false;
    }
}

bool prepare_frontend_runtime() {
    SDL_SetMainReady();

    if (SDL_WasInit(SDL_INIT_CAMERA) & SDL_INIT_CAMERA)
        return true;

    if (!SDL_InitSubSystem(SDL_INIT_CAMERA)) {
        LOG_ERROR("Failed to initialize SDL camera subsystem: {}", SDL_GetError());
        return false;
    }

    return true;
}

jint to_firmware_state_mask(const app::FirmwareState &state) {
    jint mask = 0;
    if (state.preinstalled_package)
        mask |= k_preinstalled_firmware_bit;
    if (state.main_firmware)
        mask |= k_main_firmware_bit;
    if (state.font_package)
        mask |= k_font_firmware_bit;
    return mask;
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_prepareFrontend(JNIEnv *, jclass) {
    return prepare_frontend_runtime() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_init(JNIEnv *env, jclass, jstring storage_path_str) {
    auto &session = android_session_state();

    if (session.emuenv) {
        LOG_INFO("Vita3K Android already initialised, skipping.");
        return JNI_TRUE;
    }

    const std::string storage = jstring_to_string(env, storage_path_str);
    const fs::path storage_path = fs::path(storage) / "";
    Root root_paths;
    std::unique_ptr<EmuEnvState> emuenv;
    if (!initialize_session(storage_path, root_paths, emuenv))
        return JNI_FALSE;

    session.root_paths = std::move(root_paths);
    session.emuenv = std::move(emuenv);
    session.app_session_controller = std::make_unique<app::AppSessionController>(*session.emuenv);

    LOG_INFO("Vita3K Android initialised.");
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_isInitialized(JNIEnv *, jclass) {
    return get_emuenv() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_isOfficialBuild(JNIEnv *, jclass) {
    return is_official_build ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_vita3k_emulator_NativeLib_refreshAppsList(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return;

    app::scan_apps(*emuenv);
}

JNIEXPORT jint JNICALL
Java_org_vita3k_emulator_NativeLib_getFirmwareInstallStateMask(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return 0;

    return to_firmware_state_mask(app::get_firmware_state(*emuenv));
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_getAppVersion(JNIEnv *env, jclass) {
    const std::string version = format_app_version();
    return env->NewStringUTF(version.c_str());
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_getCompatibilityDatabaseVersion(JNIEnv *env, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewStringUTF("");

    return env->NewStringUTF(emuenv->compat.db_updated_at.c_str());
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_installCompatibilityDatabase(JNIEnv *env, jclass, jbyteArray zip_bytes, jstring version_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !zip_bytes || !version_str)
        return JNI_FALSE;

    const jsize zip_size = env->GetArrayLength(zip_bytes);
    if (zip_size <= 0)
        return JNI_FALSE;

    std::vector<uint8_t> zip_data(static_cast<size_t>(zip_size));
    env->GetByteArrayRegion(zip_bytes, 0, zip_size, reinterpret_cast<jbyte *>(zip_data.data()));

    const std::string version = jstring_to_string(env, version_str);
    const bool ok = compat::install_db(
        emuenv->compat,
        std::filesystem::path(emuenv->cache_path.string()),
        std::span<const uint8_t>(zip_data.data(), zip_data.size()),
        version);

    return ok ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
