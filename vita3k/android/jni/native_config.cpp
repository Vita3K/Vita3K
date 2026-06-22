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
#include <audio/state.h>
#include <config/config.h>
#include <config/functions.h>
#include <config/settings.h>
#include <io/state.h>
#include <kernel/state.h>
#include <lang/state.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <util/log.h>

#include <SDL3/SDL_gamepad.h>

#include <algorithm>
#include <cctype>
#include <mutex>

namespace {

struct EmulatorConfigFields {
    jclass cls = nullptr;
    jmethodID ctor = nullptr;
    jfieldID modulesMode = nullptr;
    jfieldID lleModules = nullptr;
    jfieldID cpuOpt = nullptr;
    jfieldID backendRenderer = nullptr;
    jfieldID highAccuracy = nullptr;
    jfieldID resolutionMultiplier = nullptr;
    jfieldID disableSurfaceSync = nullptr;
    jfieldID screenFilter = nullptr;
    jfieldID memoryMapping = nullptr;
    jfieldID vSync = nullptr;
    jfieldID anisotropicFiltering = nullptr;
    jfieldID asyncPipelineCompilation = nullptr;
    jfieldID exportTextures = nullptr;
    jfieldID importTextures = nullptr;
    jfieldID exportAsPng = nullptr;
    jfieldID fpsHack = nullptr;
    jfieldID turboMode = nullptr;
    jfieldID shaderCache = nullptr;
    jfieldID spirvShader = nullptr;
    jfieldID customDriverName = nullptr;
    jfieldID audioBackend = nullptr;
    jfieldID audioVolume = nullptr;
    jfieldID ngsEnable = nullptr;
    jfieldID disableMotion = nullptr;
    jfieldID controllerAnalogMultiplier = nullptr;
    jfieldID controllerBinds = nullptr;
    jfieldID controllerAxisBinds = nullptr;
    jfieldID frontCameraType = nullptr;
    jfieldID frontCameraId = nullptr;
    jfieldID frontCameraImage = nullptr;
    jfieldID frontCameraColor = nullptr;
    jfieldID backCameraType = nullptr;
    jfieldID backCameraId = nullptr;
    jfieldID backCameraImage = nullptr;
    jfieldID backCameraColor = nullptr;
    jfieldID pstvMode = nullptr;
    jfieldID showMode = nullptr;
    jfieldID demoMode = nullptr;
    jfieldID sysButton = nullptr;
    jfieldID sysLang = nullptr;
    jfieldID sysDateFormat = nullptr;
    jfieldID sysTimeFormat = nullptr;
    jfieldID imeLangs = nullptr;
    jfieldID userLang = nullptr;
    jfieldID psnSignedIn = nullptr;
    jfieldID httpEnable = nullptr;
    jfieldID httpTimeoutAttempts = nullptr;
    jfieldID httpTimeoutSleepMs = nullptr;
    jfieldID httpReadEndAttempts = nullptr;
    jfieldID httpReadEndSleepMs = nullptr;
    jfieldID adhocAddr = nullptr;
    jfieldID logImports = nullptr;
    jfieldID logExports = nullptr;
    jfieldID logActiveShaders = nullptr;
    jfieldID logUniforms = nullptr;
    jfieldID colorSurfaceDebug = nullptr;
    jfieldID dumpElfs = nullptr;
    jfieldID validationLayer = nullptr;
    jfieldID textureCache = nullptr;
    jfieldID stretchDisplayArea = nullptr;
    jfieldID fullscreenHdResPixelPerfect = nullptr;
    jfieldID fileLoadingDelay = nullptr;
    jfieldID showLiveAreaScreen = nullptr;
    jfieldID showCompileShaders = nullptr;
    jfieldID checkForUpdates = nullptr;
    jfieldID checkForUpdatesMode = nullptr;
    jfieldID archiveLog = nullptr;
    jfieldID logCompatWarn = nullptr;
    jfieldID logLevel = nullptr;
    jfieldID performanceOverlay = nullptr;
    jfieldID performanceOverlayDetail = nullptr;
    jfieldID performanceOverlayPosition = nullptr;
    jfieldID screenshotFormat = nullptr;
};

EmulatorConfigFields resolve_config_fields(JNIEnv *env) {
    EmulatorConfigFields fields;

    jclass local_class = env->FindClass("org/vita3k/emulator/data/EmulatorConfig");
    if (!local_class)
        return fields;

    fields.cls = static_cast<jclass>(env->NewGlobalRef(local_class));
    env->DeleteLocalRef(local_class);
    fields.ctor = env->GetMethodID(fields.cls, "<init>", "()V");
    fields.modulesMode = env->GetFieldID(fields.cls, "modulesMode", "I");
    fields.lleModules = env->GetFieldID(fields.cls, "lleModules", "[Ljava/lang/String;");
    fields.cpuOpt = env->GetFieldID(fields.cls, "cpuOpt", "Z");
    fields.backendRenderer = env->GetFieldID(fields.cls, "backendRenderer", "Ljava/lang/String;");
    fields.highAccuracy = env->GetFieldID(fields.cls, "highAccuracy", "Z");
    fields.resolutionMultiplier = env->GetFieldID(fields.cls, "resolutionMultiplier", "F");
    fields.disableSurfaceSync = env->GetFieldID(fields.cls, "disableSurfaceSync", "Z");
    fields.screenFilter = env->GetFieldID(fields.cls, "screenFilter", "Ljava/lang/String;");
    fields.memoryMapping = env->GetFieldID(fields.cls, "memoryMapping", "Ljava/lang/String;");
    fields.vSync = env->GetFieldID(fields.cls, "vSync", "Z");
    fields.anisotropicFiltering = env->GetFieldID(fields.cls, "anisotropicFiltering", "I");
    fields.asyncPipelineCompilation = env->GetFieldID(fields.cls, "asyncPipelineCompilation", "Z");
    fields.exportTextures = env->GetFieldID(fields.cls, "exportTextures", "Z");
    fields.importTextures = env->GetFieldID(fields.cls, "importTextures", "Z");
    fields.exportAsPng = env->GetFieldID(fields.cls, "exportAsPng", "Z");
    fields.fpsHack = env->GetFieldID(fields.cls, "fpsHack", "Z");
    fields.turboMode = env->GetFieldID(fields.cls, "turboMode", "Z");
    fields.shaderCache = env->GetFieldID(fields.cls, "shaderCache", "Z");
    fields.spirvShader = env->GetFieldID(fields.cls, "spirvShader", "Z");
    fields.customDriverName = env->GetFieldID(fields.cls, "customDriverName", "Ljava/lang/String;");
    fields.audioBackend = env->GetFieldID(fields.cls, "audioBackend", "Ljava/lang/String;");
    fields.audioVolume = env->GetFieldID(fields.cls, "audioVolume", "I");
    fields.ngsEnable = env->GetFieldID(fields.cls, "ngsEnable", "Z");
    fields.disableMotion = env->GetFieldID(fields.cls, "disableMotion", "Z");
    fields.controllerAnalogMultiplier = env->GetFieldID(fields.cls, "controllerAnalogMultiplier", "F");
    fields.controllerBinds = env->GetFieldID(fields.cls, "controllerBinds", "[I");
    fields.controllerAxisBinds = env->GetFieldID(fields.cls, "controllerAxisBinds", "[I");
    fields.frontCameraType = env->GetFieldID(fields.cls, "frontCameraType", "I");
    fields.frontCameraId = env->GetFieldID(fields.cls, "frontCameraId", "Ljava/lang/String;");
    fields.frontCameraImage = env->GetFieldID(fields.cls, "frontCameraImage", "Ljava/lang/String;");
    fields.frontCameraColor = env->GetFieldID(fields.cls, "frontCameraColor", "J");
    fields.backCameraType = env->GetFieldID(fields.cls, "backCameraType", "I");
    fields.backCameraId = env->GetFieldID(fields.cls, "backCameraId", "Ljava/lang/String;");
    fields.backCameraImage = env->GetFieldID(fields.cls, "backCameraImage", "Ljava/lang/String;");
    fields.backCameraColor = env->GetFieldID(fields.cls, "backCameraColor", "J");
    fields.pstvMode = env->GetFieldID(fields.cls, "pstvMode", "Z");
    fields.showMode = env->GetFieldID(fields.cls, "showMode", "Z");
    fields.demoMode = env->GetFieldID(fields.cls, "demoMode", "Z");
    fields.sysButton = env->GetFieldID(fields.cls, "sysButton", "I");
    fields.sysLang = env->GetFieldID(fields.cls, "sysLang", "I");
    fields.sysDateFormat = env->GetFieldID(fields.cls, "sysDateFormat", "I");
    fields.sysTimeFormat = env->GetFieldID(fields.cls, "sysTimeFormat", "I");
    fields.imeLangs = env->GetFieldID(fields.cls, "imeLangs", "J");
    fields.userLang = env->GetFieldID(fields.cls, "userLang", "Ljava/lang/String;");
    fields.psnSignedIn = env->GetFieldID(fields.cls, "psnSignedIn", "Z");
    fields.httpEnable = env->GetFieldID(fields.cls, "httpEnable", "Z");
    fields.httpTimeoutAttempts = env->GetFieldID(fields.cls, "httpTimeoutAttempts", "I");
    fields.httpTimeoutSleepMs = env->GetFieldID(fields.cls, "httpTimeoutSleepMs", "I");
    fields.httpReadEndAttempts = env->GetFieldID(fields.cls, "httpReadEndAttempts", "I");
    fields.httpReadEndSleepMs = env->GetFieldID(fields.cls, "httpReadEndSleepMs", "I");
    fields.adhocAddr = env->GetFieldID(fields.cls, "adhocAddr", "I");
    fields.logImports = env->GetFieldID(fields.cls, "logImports", "Z");
    fields.logExports = env->GetFieldID(fields.cls, "logExports", "Z");
    fields.logActiveShaders = env->GetFieldID(fields.cls, "logActiveShaders", "Z");
    fields.logUniforms = env->GetFieldID(fields.cls, "logUniforms", "Z");
    fields.colorSurfaceDebug = env->GetFieldID(fields.cls, "colorSurfaceDebug", "Z");
    fields.dumpElfs = env->GetFieldID(fields.cls, "dumpElfs", "Z");
    fields.validationLayer = env->GetFieldID(fields.cls, "validationLayer", "Z");
    fields.textureCache = env->GetFieldID(fields.cls, "textureCache", "Z");
    fields.stretchDisplayArea = env->GetFieldID(fields.cls, "stretchDisplayArea", "Z");
    fields.fullscreenHdResPixelPerfect = env->GetFieldID(fields.cls, "fullscreenHdResPixelPerfect", "Z");
    fields.fileLoadingDelay = env->GetFieldID(fields.cls, "fileLoadingDelay", "I");
    fields.showLiveAreaScreen = env->GetFieldID(fields.cls, "showLiveAreaScreen", "Z");
    fields.showCompileShaders = env->GetFieldID(fields.cls, "showCompileShaders", "Z");
    fields.checkForUpdates = env->GetFieldID(fields.cls, "checkForUpdates", "Z");
    fields.checkForUpdatesMode = env->GetFieldID(fields.cls, "checkForUpdatesMode", "I");
    fields.archiveLog = env->GetFieldID(fields.cls, "archiveLog", "Z");
    fields.logCompatWarn = env->GetFieldID(fields.cls, "logCompatWarn", "Z");
    fields.logLevel = env->GetFieldID(fields.cls, "logLevel", "I");
    fields.performanceOverlay = env->GetFieldID(fields.cls, "performanceOverlay", "Z");
    fields.performanceOverlayDetail = env->GetFieldID(fields.cls, "performanceOverlayDetail", "I");
    fields.performanceOverlayPosition = env->GetFieldID(fields.cls, "performanceOverlayPosition", "I");
    fields.screenshotFormat = env->GetFieldID(fields.cls, "screenshotFormat", "I");
    return fields;
}

const EmulatorConfigFields &get_config_fields(JNIEnv *env) {
    static std::once_flag once;
    static EmulatorConfigFields fields;
    std::call_once(once, [&]() {
        fields = resolve_config_fields(env);
    });
    return fields;
}

std::vector<short> default_controller_binds() {
    return {
        SDL_GAMEPAD_BUTTON_SOUTH,
        SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST,
        SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_BACK,
        SDL_GAMEPAD_BUTTON_GUIDE,
        SDL_GAMEPAD_BUTTON_START,
        SDL_GAMEPAD_BUTTON_LEFT_STICK,
        SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_DPAD_UP,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT
    };
}

std::vector<short> default_controller_axis_binds() {
    return {
        SDL_GAMEPAD_AXIS_LEFTX,
        SDL_GAMEPAD_AXIS_LEFTY,
        SDL_GAMEPAD_AXIS_RIGHTX,
        SDL_GAMEPAD_AXIS_RIGHTY,
        SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
        SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
    };
}

const std::vector<short> &normalized_controller_binds(const Config &config) {
    static const std::vector<short> defaults = default_controller_binds();
    if (config.controller_binds.size() == defaults.size())
        return config.controller_binds;
    return defaults;
}

const std::vector<short> &normalized_controller_axis_binds(const Config &config) {
    static const std::vector<short> defaults = default_controller_axis_binds();
    if (config.controller_axis_binds.size() == defaults.size())
        return config.controller_axis_binds;
    return defaults;
}

void set_int_array_field(JNIEnv *env, jobject obj, jfieldID field, const std::vector<short> &values) {
    jintArray array = env->NewIntArray(static_cast<jsize>(values.size()));
    if (!array)
        return;

    std::vector<jint> ints(values.begin(), values.end());
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(ints.size()), ints.data());
    env->SetObjectField(obj, field, array);
    env->DeleteLocalRef(array);
}

void read_int_array_field(JNIEnv *env, jobject obj, jfieldID field, std::vector<short> &out_values) {
    auto *array = reinterpret_cast<jintArray>(env->GetObjectField(obj, field));
    if (!array)
        return;

    const jsize length = env->GetArrayLength(array);
    std::vector<jint> ints(static_cast<size_t>(length));
    env->GetIntArrayRegion(array, 0, length, ints.data());
    out_values.resize(static_cast<size_t>(length));
    for (jsize index = 0; index < length; ++index)
        out_values[static_cast<size_t>(index)] = static_cast<short>(ints[static_cast<size_t>(index)]);
    env->DeleteLocalRef(array);
}

void fill_config_object(JNIEnv *env, jobject obj, const EmulatorConfigFields &fields,
    const Config::CurrentConfig &current_config, const Config &config, const EmuEnvState *emuenv) {
    env->SetIntField(obj, fields.modulesMode, static_cast<jint>(current_config.modules_mode));
    {
        jclass string_class = env->FindClass("java/lang/String");
        jobjectArray modules_array = env->NewObjectArray(static_cast<jsize>(current_config.lle_modules.size()), string_class, nullptr);
        for (jsize index = 0; index < static_cast<jsize>(current_config.lle_modules.size()); ++index) {
            jstring module = env->NewStringUTF(current_config.lle_modules[static_cast<size_t>(index)].c_str());
            env->SetObjectArrayElement(modules_array, index, module);
            env->DeleteLocalRef(module);
        }
        env->SetObjectField(obj, fields.lleModules, modules_array);
        env->DeleteLocalRef(modules_array);
        env->DeleteLocalRef(string_class);
    }
    env->SetBooleanField(obj, fields.cpuOpt, current_config.cpu_opt);
    {
        jstring value = env->NewStringUTF(current_config.backend_renderer.c_str());
        env->SetObjectField(obj, fields.backendRenderer, value);
        env->DeleteLocalRef(value);
    }
    env->SetBooleanField(obj, fields.highAccuracy, current_config.high_accuracy);
    env->SetFloatField(obj, fields.resolutionMultiplier, current_config.resolution_multiplier);
    env->SetBooleanField(obj, fields.disableSurfaceSync, current_config.disable_surface_sync);
    {
        jstring value = env->NewStringUTF(current_config.screen_filter.c_str());
        env->SetObjectField(obj, fields.screenFilter, value);
        env->DeleteLocalRef(value);
    }
    {
        jstring value = env->NewStringUTF(current_config.memory_mapping.c_str());
        env->SetObjectField(obj, fields.memoryMapping, value);
        env->DeleteLocalRef(value);
    }
    env->SetBooleanField(obj, fields.vSync, current_config.v_sync);
    env->SetIntField(obj, fields.anisotropicFiltering, static_cast<jint>(current_config.anisotropic_filtering));
    env->SetBooleanField(obj, fields.asyncPipelineCompilation, current_config.async_pipeline_compilation);
    env->SetBooleanField(obj, fields.exportTextures, current_config.export_textures);
    env->SetBooleanField(obj, fields.importTextures, current_config.import_textures);
    env->SetBooleanField(obj, fields.exportAsPng, current_config.export_as_png);
    env->SetBooleanField(obj, fields.fpsHack, current_config.fps_hack);
    env->SetBooleanField(obj, fields.turboMode, config.turbo_mode);
    env->SetBooleanField(obj, fields.shaderCache, current_config.shader_cache);
    env->SetBooleanField(obj, fields.spirvShader, current_config.spirv_shader);
    {
        jstring value = env->NewStringUTF(current_config.custom_driver_name.c_str());
        env->SetObjectField(obj, fields.customDriverName, value);
        env->DeleteLocalRef(value);
    }
    {
        jstring value = env->NewStringUTF(current_config.audio_backend.c_str());
        env->SetObjectField(obj, fields.audioBackend, value);
        env->DeleteLocalRef(value);
    }
    env->SetIntField(obj, fields.audioVolume, static_cast<jint>(current_config.audio_volume));
    env->SetBooleanField(obj, fields.ngsEnable, current_config.ngs_enable);
    env->SetBooleanField(obj, fields.disableMotion, config.disable_motion);
    env->SetFloatField(obj, fields.controllerAnalogMultiplier, config.controller_analog_multiplier);
    set_int_array_field(env, obj, fields.controllerBinds, normalized_controller_binds(config));
    set_int_array_field(env, obj, fields.controllerAxisBinds, normalized_controller_axis_binds(config));
    env->SetIntField(obj, fields.frontCameraType, static_cast<jint>(config.front_camera_type));
    {
        jstring value = env->NewStringUTF(config.front_camera_id.c_str());
        env->SetObjectField(obj, fields.frontCameraId, value);
        env->DeleteLocalRef(value);
    }
    {
        jstring value = env->NewStringUTF(config.front_camera_image.c_str());
        env->SetObjectField(obj, fields.frontCameraImage, value);
        env->DeleteLocalRef(value);
    }
    env->SetLongField(obj, fields.frontCameraColor, static_cast<jlong>(config.front_camera_color));
    env->SetIntField(obj, fields.backCameraType, static_cast<jint>(config.back_camera_type));
    {
        jstring value = env->NewStringUTF(config.back_camera_id.c_str());
        env->SetObjectField(obj, fields.backCameraId, value);
        env->DeleteLocalRef(value);
    }
    {
        jstring value = env->NewStringUTF(config.back_camera_image.c_str());
        env->SetObjectField(obj, fields.backCameraImage, value);
        env->DeleteLocalRef(value);
    }
    env->SetLongField(obj, fields.backCameraColor, static_cast<jlong>(config.back_camera_color));
    env->SetBooleanField(obj, fields.pstvMode, current_config.pstv_mode);
    env->SetBooleanField(obj, fields.showMode, config.show_mode);
    env->SetBooleanField(obj, fields.demoMode, config.demo_mode);
    env->SetIntField(obj, fields.sysButton, static_cast<jint>(current_config.sys_button));
    env->SetIntField(obj, fields.sysLang, static_cast<jint>(current_config.sys_lang));
    env->SetIntField(obj, fields.sysDateFormat, static_cast<jint>(current_config.sys_date_format));
    env->SetIntField(obj, fields.sysTimeFormat, static_cast<jint>(current_config.sys_time_format));
    {
        uint64_t ime_mask = 0;
        for (const auto &language : current_config.ime_langs)
            ime_mask |= language;
        env->SetLongField(obj, fields.imeLangs, static_cast<jlong>(ime_mask));
    }
    {
        jstring value = env->NewStringUTF(config.user_lang.c_str());
        env->SetObjectField(obj, fields.userLang, value);
        env->DeleteLocalRef(value);
    }
    env->SetBooleanField(obj, fields.psnSignedIn, current_config.psn_signed_in);
    env->SetBooleanField(obj, fields.httpEnable, config.http_enable);
    env->SetIntField(obj, fields.httpTimeoutAttempts, static_cast<jint>(config.http_timeout_attempts));
    env->SetIntField(obj, fields.httpTimeoutSleepMs, static_cast<jint>(config.http_timeout_sleep_ms));
    env->SetIntField(obj, fields.httpReadEndAttempts, static_cast<jint>(config.http_read_end_attempts));
    env->SetIntField(obj, fields.httpReadEndSleepMs, static_cast<jint>(config.http_read_end_sleep_ms));
    env->SetIntField(obj, fields.adhocAddr, static_cast<jint>(config.adhoc_addr));
    env->SetBooleanField(obj, fields.logImports, emuenv && emuenv->kernel.debugger.log_imports);
    env->SetBooleanField(obj, fields.logExports, emuenv && emuenv->kernel.debugger.log_exports);
    env->SetBooleanField(obj, fields.logActiveShaders, current_config.log_active_shaders);
    env->SetBooleanField(obj, fields.logUniforms, current_config.log_uniforms);
    env->SetBooleanField(obj, fields.colorSurfaceDebug, current_config.color_surface_debug);
    env->SetBooleanField(obj, fields.dumpElfs, emuenv && emuenv->kernel.debugger.dump_elfs);
    env->SetBooleanField(obj, fields.validationLayer, current_config.validation_layer);
    env->SetBooleanField(obj, fields.textureCache, current_config.texture_cache);
    env->SetBooleanField(obj, fields.stretchDisplayArea, current_config.stretch_the_display_area);
    env->SetBooleanField(obj, fields.fullscreenHdResPixelPerfect, current_config.fullscreen_hd_res_pixel_perfect);
    env->SetIntField(obj, fields.fileLoadingDelay, static_cast<jint>(current_config.file_loading_delay));
    env->SetBooleanField(obj, fields.showLiveAreaScreen, config.show_live_area_screen);
    env->SetBooleanField(obj, fields.showCompileShaders, config.show_compile_shaders);
    env->SetBooleanField(obj, fields.checkForUpdates, config.check_for_updates);
    env->SetIntField(obj, fields.checkForUpdatesMode, static_cast<jint>(config.check_for_updates_mode));
    env->SetBooleanField(obj, fields.archiveLog, config.archive_log);
    env->SetBooleanField(obj, fields.logCompatWarn, config.log_compat_warn);
    env->SetIntField(obj, fields.logLevel, static_cast<jint>(config.log_level));
    env->SetBooleanField(obj, fields.performanceOverlay, config.performance_overlay);
    env->SetIntField(obj, fields.performanceOverlayDetail, static_cast<jint>(config.performance_overlay_detail));
    env->SetIntField(obj, fields.performanceOverlayPosition, static_cast<jint>(config.performance_overlay_position));
    env->SetIntField(obj, fields.screenshotFormat, static_cast<jint>(config.screenshot_format));
}

void read_config_object(JNIEnv *env, jobject obj, const EmulatorConfigFields &fields,
    Config::CurrentConfig &current_config, Config &config, EmuEnvState *emuenv = nullptr, bool apply_runtime_settings = false) {
    current_config.modules_mode = static_cast<int>(env->GetIntField(obj, fields.modulesMode));
    {
        auto *modules_array = reinterpret_cast<jobjectArray>(env->GetObjectField(obj, fields.lleModules));
        if (modules_array) {
            const jsize length = env->GetArrayLength(modules_array);
            current_config.lle_modules.clear();
            for (jsize index = 0; index < length; ++index) {
                auto *module = reinterpret_cast<jstring>(env->GetObjectArrayElement(modules_array, index));
                if (module) {
                    current_config.lle_modules.push_back(jstring_to_string(env, module));
                    env->DeleteLocalRef(module);
                }
            }
            env->DeleteLocalRef(modules_array);
        }
    }
    current_config.cpu_opt = env->GetBooleanField(obj, fields.cpuOpt) != JNI_FALSE;
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.backendRenderer));
        if (value) {
            current_config.backend_renderer = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    current_config.high_accuracy = env->GetBooleanField(obj, fields.highAccuracy) != JNI_FALSE;
    current_config.resolution_multiplier = env->GetFloatField(obj, fields.resolutionMultiplier);
    current_config.disable_surface_sync = env->GetBooleanField(obj, fields.disableSurfaceSync) != JNI_FALSE;
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.screenFilter));
        if (value) {
            current_config.screen_filter = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.memoryMapping));
        if (value) {
            current_config.memory_mapping = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    current_config.v_sync = env->GetBooleanField(obj, fields.vSync) != JNI_FALSE;
    current_config.anisotropic_filtering = static_cast<int>(env->GetIntField(obj, fields.anisotropicFiltering));
    current_config.async_pipeline_compilation = env->GetBooleanField(obj, fields.asyncPipelineCompilation) != JNI_FALSE;
    current_config.export_textures = env->GetBooleanField(obj, fields.exportTextures) != JNI_FALSE;
    current_config.import_textures = env->GetBooleanField(obj, fields.importTextures) != JNI_FALSE;
    current_config.export_as_png = env->GetBooleanField(obj, fields.exportAsPng) != JNI_FALSE;
    current_config.fps_hack = env->GetBooleanField(obj, fields.fpsHack) != JNI_FALSE;
    config.turbo_mode = env->GetBooleanField(obj, fields.turboMode) != JNI_FALSE;
    current_config.shader_cache = env->GetBooleanField(obj, fields.shaderCache) != JNI_FALSE;
    current_config.spirv_shader = env->GetBooleanField(obj, fields.spirvShader) != JNI_FALSE;
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.customDriverName));
        if (value) {
            current_config.custom_driver_name = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.audioBackend));
        if (value) {
            current_config.audio_backend = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    current_config.audio_volume = static_cast<int>(env->GetIntField(obj, fields.audioVolume));
    current_config.ngs_enable = env->GetBooleanField(obj, fields.ngsEnable) != JNI_FALSE;
    config.disable_motion = env->GetBooleanField(obj, fields.disableMotion) != JNI_FALSE;
    config.controller_analog_multiplier = env->GetFloatField(obj, fields.controllerAnalogMultiplier);
    read_int_array_field(env, obj, fields.controllerBinds, config.controller_binds);
    read_int_array_field(env, obj, fields.controllerAxisBinds, config.controller_axis_binds);
    config.front_camera_type = static_cast<int>(env->GetIntField(obj, fields.frontCameraType));
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.frontCameraId));
        if (value) {
            config.front_camera_id = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.frontCameraImage));
        if (value) {
            config.front_camera_image = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    config.front_camera_color = static_cast<uint32_t>(env->GetLongField(obj, fields.frontCameraColor));
    config.back_camera_type = static_cast<int>(env->GetIntField(obj, fields.backCameraType));
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.backCameraId));
        if (value) {
            config.back_camera_id = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.backCameraImage));
        if (value) {
            config.back_camera_image = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        }
    }
    config.back_camera_color = static_cast<uint32_t>(env->GetLongField(obj, fields.backCameraColor));
    current_config.pstv_mode = env->GetBooleanField(obj, fields.pstvMode) != JNI_FALSE;
    config.show_mode = env->GetBooleanField(obj, fields.showMode) != JNI_FALSE;
    config.demo_mode = env->GetBooleanField(obj, fields.demoMode) != JNI_FALSE;
    current_config.sys_button = static_cast<int>(env->GetIntField(obj, fields.sysButton));
    current_config.sys_lang = static_cast<int>(env->GetIntField(obj, fields.sysLang));
    current_config.sys_date_format = static_cast<int>(env->GetIntField(obj, fields.sysDateFormat));
    current_config.sys_time_format = static_cast<int>(env->GetIntField(obj, fields.sysTimeFormat));
    current_config.ime_langs = { static_cast<uint64_t>(env->GetLongField(obj, fields.imeLangs)) };
    {
        auto *value = reinterpret_cast<jstring>(env->GetObjectField(obj, fields.userLang));
        if (value) {
            config.user_lang = jstring_to_string(env, value);
            env->DeleteLocalRef(value);
        } else {
            config.user_lang.clear();
        }
    }
    current_config.psn_signed_in = env->GetBooleanField(obj, fields.psnSignedIn) != JNI_FALSE;
    config.http_enable = env->GetBooleanField(obj, fields.httpEnable) != JNI_FALSE;
    config.http_timeout_attempts = static_cast<int>(env->GetIntField(obj, fields.httpTimeoutAttempts));
    config.http_timeout_sleep_ms = static_cast<int>(env->GetIntField(obj, fields.httpTimeoutSleepMs));
    config.http_read_end_attempts = static_cast<int>(env->GetIntField(obj, fields.httpReadEndAttempts));
    config.http_read_end_sleep_ms = static_cast<int>(env->GetIntField(obj, fields.httpReadEndSleepMs));
    config.adhoc_addr = static_cast<int>(env->GetIntField(obj, fields.adhocAddr));
    current_config.log_active_shaders = env->GetBooleanField(obj, fields.logActiveShaders) != JNI_FALSE;
    current_config.log_uniforms = env->GetBooleanField(obj, fields.logUniforms) != JNI_FALSE;
    current_config.color_surface_debug = env->GetBooleanField(obj, fields.colorSurfaceDebug) != JNI_FALSE;
    current_config.validation_layer = env->GetBooleanField(obj, fields.validationLayer) != JNI_FALSE;
    current_config.texture_cache = env->GetBooleanField(obj, fields.textureCache) != JNI_FALSE;
    current_config.stretch_the_display_area = env->GetBooleanField(obj, fields.stretchDisplayArea) != JNI_FALSE;
    current_config.fullscreen_hd_res_pixel_perfect = env->GetBooleanField(obj, fields.fullscreenHdResPixelPerfect) != JNI_FALSE;
    current_config.file_loading_delay = static_cast<int>(env->GetIntField(obj, fields.fileLoadingDelay));
    config.show_live_area_screen = env->GetBooleanField(obj, fields.showLiveAreaScreen) != JNI_FALSE;
    config.show_compile_shaders = env->GetBooleanField(obj, fields.showCompileShaders) != JNI_FALSE;
    config.check_for_updates_mode = static_cast<int>(env->GetIntField(obj, fields.checkForUpdatesMode));
    config.check_for_updates = config.check_for_updates_mode != static_cast<int>(UPDATE_STARTUP_OFF);
    config.archive_log = env->GetBooleanField(obj, fields.archiveLog) != JNI_FALSE;
    config.log_compat_warn = env->GetBooleanField(obj, fields.logCompatWarn) != JNI_FALSE;
    config.log_level = static_cast<int>(env->GetIntField(obj, fields.logLevel));
    config.performance_overlay = env->GetBooleanField(obj, fields.performanceOverlay) != JNI_FALSE;
    config.performance_overlay_detail = static_cast<int>(env->GetIntField(obj, fields.performanceOverlayDetail));
    config.performance_overlay_position = static_cast<int>(env->GetIntField(obj, fields.performanceOverlayPosition));
    if (apply_runtime_settings && emuenv) {
        emuenv->kernel.debugger.log_imports = env->GetBooleanField(obj, fields.logImports) != JNI_FALSE;
        emuenv->kernel.debugger.log_exports = env->GetBooleanField(obj, fields.logExports) != JNI_FALSE;
        emuenv->kernel.debugger.dump_elfs = env->GetBooleanField(obj, fields.dumpElfs) != JNI_FALSE;
    }
    config.screenshot_format = static_cast<int>(env->GetIntField(obj, fields.screenshotFormat));
}

jintArray make_restart_required_array(JNIEnv *env, const std::vector<config::RestartRequiredSetting> &values) {
    jintArray result = env->NewIntArray(static_cast<jsize>(values.size()));
    if (!result)
        return nullptr;

    std::vector<jint> ints(values.size());
    std::transform(values.begin(), values.end(), ints.begin(), [](config::RestartRequiredSetting setting) {
        return static_cast<jint>(setting);
    });
    env->SetIntArrayRegion(result, 0, static_cast<jsize>(ints.size()), ints.data());
    return result;
}

renderer::VulkanDeviceInfo *get_cached_default_vulkan_device_info(EmuEnvState *emuenv) {
    if (!emuenv)
        return nullptr;

    const bool missing_default_probe = !emuenv->vulkan_device_info
        || (emuenv->vulkan_device_info->gpu_names.size() <= 1 && emuenv->vulkan_device_info->mapping_method_masks.empty());

    if (missing_default_probe)
        emuenv->vulkan_device_info = std::make_unique<renderer::VulkanDeviceInfo>(renderer::enumerate_vulkan_devices());

    return emuenv->vulkan_device_info.get();
}

int get_default_supported_memory_mapping_mask(EmuEnvState *emuenv) {
    if (emuenv && emuenv->renderer && emuenv->backend_renderer == renderer::Backend::Vulkan)
        return emuenv->renderer->supported_mapping_methods_mask;

    if (const auto *device_info = get_cached_default_vulkan_device_info(emuenv)) {
        if (!device_info->mapping_method_masks.empty())
            return device_info->mapping_method_masks.front();
    }

    return static_cast<int>(1 << 0);
}

bool default_gpu_is_mali(EmuEnvState *emuenv) {
    std::string gpu_name;
    if (emuenv && emuenv->renderer && emuenv->backend_renderer == renderer::Backend::Vulkan) {
        gpu_name = std::string(emuenv->renderer->get_gpu_name());
    } else if (const auto *device_info = get_cached_default_vulkan_device_info(emuenv)) {
        if (device_info->gpu_names.size() > 1)
            gpu_name = device_info->gpu_names[1];
    }

    std::transform(gpu_name.begin(), gpu_name.end(), gpu_name.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return gpu_name.find("mali") != std::string::npos;
}

} // namespace

extern "C" {

JNIEXPORT jobject JNICALL
Java_org_vita3k_emulator_NativeLib_getGlobalConfig(JNIEnv *env, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return nullptr;

    Config cfg_copy;
    cfg_copy = emuenv->cfg;
    config::set_current_config(cfg_copy, emuenv->config_path, {});
    app::ensure_camera_defaults(cfg_copy);
    const auto &fields = get_config_fields(env);
    if (!fields.cls || !fields.ctor)
        return nullptr;

    jobject config_object = env->NewObject(fields.cls, fields.ctor);
    fill_config_object(env, config_object, fields, cfg_copy.current_config, cfg_copy, emuenv);
    return config_object;
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_getCurrentEmulatorPath(JNIEnv *env, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewStringUTF("");

    const std::string vita_fs_path = emuenv->vita_fs_path.generic_string();
    return env->NewStringUTF(vita_fs_path.c_str());
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_setCurrentEmulatorPath(JNIEnv *env, jclass, jstring path_str) {
    auto *emuenv = get_emuenv();
    auto *controller = get_app_session_controller();
    if (!emuenv || !path_str || (controller && controller->has_active_session()))
        return JNI_FALSE;

    const std::string path = jstring_to_string(env, path_str);
    if (path.empty())
        return JNI_FALSE;

    if (!app::switch_emulator_path(*emuenv, fs::path(path)))
        return JNI_FALSE;
    config::save_current_config(emuenv->cfg, emuenv->config_path, {});
    return JNI_TRUE;
}

JNIEXPORT jintArray JNICALL
Java_org_vita3k_emulator_NativeLib_saveSettings(JNIEnv *env, jclass, jstring title_id_str, jobject config_obj) {
    auto *emuenv = get_emuenv();
    if (!emuenv || !config_obj)
        return make_restart_required_array(env, {});

    const auto &fields = get_config_fields(env);
    if (!fields.cls)
        return make_restart_required_array(env, {});

    const std::string title_id = title_id_str ? jstring_to_string(env, title_id_str) : std::string();
    Config desired_cfg;
    desired_cfg = emuenv->cfg;
    read_config_object(env, config_obj, fields, desired_cfg.current_config, desired_cfg);

    const int previous_log_level = emuenv->cfg.log_level;
    const auto result = app::commit_settings(*emuenv, desired_cfg, title_id);

    if (title_id.empty()) {
        if (emuenv->cfg.log_level != previous_log_level)
            logging::set_level(static_cast<spdlog::level::level_enum>(emuenv->cfg.log_level));

        emuenv->kernel.debugger.log_imports = env->GetBooleanField(config_obj, fields.logImports) != JNI_FALSE;
        emuenv->kernel.debugger.log_exports = env->GetBooleanField(config_obj, fields.logExports) != JNI_FALSE;
        emuenv->kernel.debugger.dump_elfs = env->GetBooleanField(config_obj, fields.dumpElfs) != JNI_FALSE;
    }

    return make_restart_required_array(env, result.restart_required_settings);
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_hasCustomConfig(JNIEnv *env, jclass, jstring title_id_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return JNI_FALSE;

    const std::string title_id = jstring_to_string(env, title_id_str);
    return config::has_custom_config(emuenv->config_path, title_id) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
Java_org_vita3k_emulator_NativeLib_getCustomConfig(JNIEnv *env, jclass, jstring title_id_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return nullptr;

    const std::string title_id = jstring_to_string(env, title_id_str);
    if (!config::has_custom_config(emuenv->config_path, title_id))
        return nullptr;

    Config cfg_copy;
    cfg_copy = emuenv->cfg;
    config::set_current_config(cfg_copy, emuenv->config_path, title_id);
    app::ensure_camera_defaults(cfg_copy);

    const auto &fields = get_config_fields(env);
    if (!fields.cls || !fields.ctor)
        return nullptr;

    jobject config_object = env->NewObject(fields.cls, fields.ctor);
    fill_config_object(env, config_object, fields, cfg_copy.current_config, cfg_copy, emuenv);
    return config_object;
}

JNIEXPORT jintArray JNICALL
Java_org_vita3k_emulator_NativeLib_deleteCustomConfig(JNIEnv *env, jclass, jstring title_id_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return make_restart_required_array(env, {});

    const std::string title_id = jstring_to_string(env, title_id_str);
    const auto result = app::delete_custom_settings(*emuenv, title_id);
    return make_restart_required_array(env, result.restart_required_settings);
}

JNIEXPORT jobject JNICALL
Java_org_vita3k_emulator_NativeLib_getDefaultConfig(JNIEnv *env, jclass) {
    const auto &fields = get_config_fields(env);
    if (!fields.cls || !fields.ctor)
        return nullptr;

    Config default_config;
    config::set_current_config(default_config, {}, {});
    app::ensure_camera_defaults(default_config);

    jobject config_object = env->NewObject(fields.cls, fields.ctor);
    fill_config_object(env, config_object, fields, default_config.current_config, default_config, nullptr);
    return config_object;
}

JNIEXPORT jint JNICALL
Java_org_vita3k_emulator_NativeLib_clearAllCustomConfigs(JNIEnv *, jclass) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return 0;

    return static_cast<jint>(config::delete_all_custom_configs(emuenv->config_path));
}

JNIEXPORT jobjectArray JNICALL
Java_org_vita3k_emulator_NativeLib_getModulesList(JNIEnv *env, jclass, jobjectArray current_modules_arr) {
    jclass string_class = env->FindClass("java/lang/String");
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewObjectArray(0, string_class, nullptr);

    std::vector<std::string> current_modules;
    const jsize length = env->GetArrayLength(current_modules_arr);
    for (jsize index = 0; index < length; ++index) {
        auto *module = reinterpret_cast<jstring>(env->GetObjectArrayElement(current_modules_arr, index));
        if (module) {
            current_modules.push_back(jstring_to_string(env, module));
            env->DeleteLocalRef(module);
        }
    }

    const auto modules = config::get_modules_list(emuenv->vita_fs_path, current_modules);
    jobjectArray result = env->NewObjectArray(static_cast<jsize>(modules.size() * 2), string_class, nullptr);
    for (jsize index = 0; index < static_cast<jsize>(modules.size()); ++index) {
        jstring name = env->NewStringUTF(modules[index].first.c_str());
        jstring enabled = env->NewStringUTF(modules[index].second ? "true" : "false");
        env->SetObjectArrayElement(result, index * 2, name);
        env->SetObjectArrayElement(result, index * 2 + 1, enabled);
        env->DeleteLocalRef(name);
        env->DeleteLocalRef(enabled);
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_org_vita3k_emulator_NativeLib_getSupportedMemoryMappingMask(JNIEnv *env, jclass, jstring custom_driver_name_str) {
    const std::string custom_driver_name = custom_driver_name_str ? jstring_to_string(env, custom_driver_name_str) : std::string();
    if (custom_driver_name.empty())
        return static_cast<jint>(get_default_supported_memory_mapping_mask(get_emuenv()));

    const auto device_info = renderer::enumerate_vulkan_devices(custom_driver_name);
    if (device_info.mapping_method_masks.empty())
        return static_cast<jint>(1 << 0);

    return static_cast<jint>(device_info.mapping_method_masks.front());
}

JNIEXPORT jintArray JNICALL
Java_org_vita3k_emulator_NativeLib_getCustomDriverSupportInfo(JNIEnv *env, jclass, jstring custom_driver_name_str) {
    const std::string custom_driver_name = custom_driver_name_str ? jstring_to_string(env, custom_driver_name_str) : std::string();
    if (custom_driver_name.empty()) {
        jint values[2] = {
            static_cast<jint>(get_default_supported_memory_mapping_mask(get_emuenv())),
            0
        };

        jintArray result = env->NewIntArray(2);
        env->SetIntArrayRegion(result, 0, 2, values);
        return result;
    }

    const auto device_info = renderer::enumerate_vulkan_devices(custom_driver_name);

    jint values[2] = {
        device_info.mapping_method_masks.empty() ? static_cast<jint>(1 << 0) : static_cast<jint>(device_info.mapping_method_masks.front()),
        static_cast<jint>(device_info.custom_driver_requested ? (device_info.custom_driver_loaded ? 1 : 2) : 0)
    };

    jintArray result = env->NewIntArray(2);
    env->SetIntArrayRegion(result, 0, 2, values);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_shouldShowCustomDriverOptions(JNIEnv *, jclass) {
    return default_gpu_is_mali(get_emuenv()) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_org_vita3k_emulator_NativeLib_getAvailableCameras(JNIEnv *env, jclass) {
    jclass string_class = env->FindClass("java/lang/String");
    const auto camera_names = app::get_available_camera_names();

    jobjectArray result = env->NewObjectArray(static_cast<jsize>(camera_names.size()), string_class, nullptr);
    for (jsize index = 0; index < static_cast<jsize>(camera_names.size()); ++index) {
        jstring value = env->NewStringUTF(camera_names[static_cast<size_t>(index)].c_str());
        env->SetObjectArrayElement(result, index, value);
        env->DeleteLocalRef(value);
    }
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_vita3k_emulator_NativeLib_getAvailableAdhocAddresses(JNIEnv *env, jclass) {
    jclass string_class = env->FindClass("java/lang/String");
    const auto addrs = app::get_available_adhoc_address_options();
    jobjectArray result = env->NewObjectArray(static_cast<jsize>(addrs.size() * 2), string_class, nullptr);
    for (jsize index = 0; index < static_cast<jsize>(addrs.size()); ++index) {
        const auto &addr = addrs[static_cast<size_t>(index)];
        jstring label_value = env->NewStringUTF(addr.label.c_str());
        jstring mask_value = env->NewStringUTF(addr.subnet_mask.c_str());
        env->SetObjectArrayElement(result, index * 2, label_value);
        env->SetObjectArrayElement(result, index * 2 + 1, mask_value);
        env->DeleteLocalRef(label_value);
        env->DeleteLocalRef(mask_value);
    }
    return result;
}

} // extern "C"
