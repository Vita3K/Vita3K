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

#include <config/settings.h>

#include <config/functions.h>

#include <util/log.h>
#include <util/vector_utils.h>

#include <pugixml.hpp>

#include <algorithm>

namespace config {

namespace {

void copy_global_to_current(Config::CurrentConfig &current, const Config &cfg) {
    current.cpu_opt = cfg.cpu_opt;
    current.modules_mode = cfg.modules_mode;
    current.lle_modules = cfg.lle_modules;
    current.backend_renderer = cfg.backend_renderer;
    current.gpu_idx = cfg.gpu_idx;
#ifdef __ANDROID__
    current.custom_driver_name = cfg.custom_driver_name;
#endif
    current.high_accuracy = cfg.high_accuracy;
    current.resolution_multiplier = cfg.resolution_multiplier;
    current.disable_surface_sync = cfg.disable_surface_sync;
    current.screen_filter = cfg.screen_filter;
    current.memory_mapping = cfg.memory_mapping;
    current.v_sync = cfg.v_sync;
    current.anisotropic_filtering = cfg.anisotropic_filtering;
    current.async_pipeline_compilation = cfg.async_pipeline_compilation;
    current.import_textures = cfg.import_textures;
    current.export_textures = cfg.export_textures;
    current.export_as_png = cfg.export_as_png;
    current.fps_hack = cfg.fps_hack;
    current.shader_cache = cfg.shader_cache;
    current.spirv_shader = cfg.spirv_shader;
    current.texture_cache = cfg.texture_cache;
    current.audio_backend = cfg.audio_backend;
    current.audio_volume = cfg.audio_volume;
    current.ngs_enable = cfg.ngs_enable;
    current.pstv_mode = cfg.pstv_mode;
    current.stretch_the_display_area = cfg.stretch_the_display_area;
    current.fullscreen_hd_res_pixel_perfect = cfg.fullscreen_hd_res_pixel_perfect;
    current.file_loading_delay = cfg.file_loading_delay;
    current.psn_signed_in = cfg.psn_signed_in;
    current.sys_button = cfg.sys_button;
    current.sys_lang = cfg.sys_lang;
    current.sys_date_format = cfg.sys_date_format;
    current.sys_time_format = cfg.sys_time_format;
    current.ime_langs = cfg.ime_langs;
    current.log_active_shaders = cfg.log_active_shaders;
    current.log_uniforms = cfg.log_uniforms;
    current.color_surface_debug = cfg.color_surface_debug;
    current.validation_layer = cfg.validation_layer;
    current.tracy_primitive_impl = cfg.tracy_primitive_impl;
    current.tracy_advanced_profiling_modules = cfg.tracy_advanced_profiling_modules;
}

void copy_current_to_global(Config &cfg, const Config::CurrentConfig &current) {
    cfg.cpu_opt = current.cpu_opt;
    cfg.modules_mode = current.modules_mode;
    cfg.lle_modules = current.lle_modules;
    cfg.backend_renderer = current.backend_renderer;
    cfg.gpu_idx = current.gpu_idx;
#ifdef __ANDROID__
    cfg.custom_driver_name = current.custom_driver_name;
#endif
    cfg.high_accuracy = current.high_accuracy;
    cfg.resolution_multiplier = current.resolution_multiplier;
    cfg.disable_surface_sync = current.disable_surface_sync;
    cfg.screen_filter = current.screen_filter;
    cfg.memory_mapping = current.memory_mapping;
    cfg.v_sync = current.v_sync;
    cfg.anisotropic_filtering = current.anisotropic_filtering;
    cfg.async_pipeline_compilation = current.async_pipeline_compilation;
    cfg.import_textures = current.import_textures;
    cfg.export_textures = current.export_textures;
    cfg.export_as_png = current.export_as_png;
    cfg.fps_hack = current.fps_hack;
    cfg.shader_cache = current.shader_cache;
    cfg.spirv_shader = current.spirv_shader;
    cfg.texture_cache = current.texture_cache;
    cfg.audio_backend = current.audio_backend;
    cfg.audio_volume = current.audio_volume;
    cfg.ngs_enable = current.ngs_enable;
    cfg.pstv_mode = current.pstv_mode;
    cfg.stretch_the_display_area = current.stretch_the_display_area;
    cfg.fullscreen_hd_res_pixel_perfect = current.fullscreen_hd_res_pixel_perfect;
    cfg.file_loading_delay = current.file_loading_delay;
    cfg.psn_signed_in = current.psn_signed_in;
    cfg.sys_button = current.sys_button;
    cfg.sys_lang = current.sys_lang;
    cfg.sys_date_format = current.sys_date_format;
    cfg.sys_time_format = current.sys_time_format;
    cfg.ime_langs = current.ime_langs;
    cfg.log_active_shaders = current.log_active_shaders;
    cfg.log_uniforms = current.log_uniforms;
    cfg.color_surface_debug = current.color_surface_debug;
    cfg.validation_layer = current.validation_layer;
    cfg.tracy_primitive_impl = current.tracy_primitive_impl;
    cfg.tracy_advanced_profiling_modules = current.tracy_advanced_profiling_modules;
}

} // namespace

std::vector<RestartRequiredSetting> get_restart_required_settings(
    const Config::CurrentConfig &before,
    const Config::CurrentConfig &after) {
    std::vector<RestartRequiredSetting> changed;

    const auto append_if_changed = [&](bool has_changed, RestartRequiredSetting setting) {
        if (has_changed)
            changed.emplace_back(setting);
    };

    append_if_changed(before.cpu_opt != after.cpu_opt, RestartRequiredSetting::CpuOpt);
    append_if_changed(before.backend_renderer != after.backend_renderer, RestartRequiredSetting::BackendRenderer);
    append_if_changed(before.gpu_idx != after.gpu_idx, RestartRequiredSetting::GraphicsDevice);
#ifdef __ANDROID__
    append_if_changed(before.custom_driver_name != after.custom_driver_name, RestartRequiredSetting::CustomDriver);
#endif
    append_if_changed(before.high_accuracy != after.high_accuracy, RestartRequiredSetting::HighAccuracy);
    append_if_changed(before.resolution_multiplier != after.resolution_multiplier, RestartRequiredSetting::ResolutionMultiplier);
    append_if_changed(before.memory_mapping != after.memory_mapping, RestartRequiredSetting::MemoryMapping);
    append_if_changed(before.audio_backend != after.audio_backend, RestartRequiredSetting::AudioBackend);
    append_if_changed(before.validation_layer != after.validation_layer, RestartRequiredSetting::ValidationLayer);

    return changed;
}

static fs::path get_custom_config_path(const fs::path &config_path, const std::string &app_path) {
    return config_path / "config" / fmt::format("config_{}.xml", app_path);
}

bool load_custom_config(Config::CurrentConfig &out, const fs::path &config_path, const std::string &app_path) {
    if (app_path.empty())
        return false;

    const auto custom_cfg_path = get_custom_config_path(config_path, app_path);
    if (!fs::exists(custom_cfg_path))
        return false;

    pugi::xml_document doc;
    if (!doc.load_file(custom_cfg_path.c_str()) || doc.child("config").empty()) {
        LOG_ERROR("Custom config at {} is corrupted or invalid.", custom_cfg_path);
        fs::remove(custom_cfg_path);
        return false;
    }

    const auto config_child = doc.child("config");

    if (!config_child.child("core").empty()) {
        const auto core = config_child.child("core");
        out.modules_mode = core.attribute("modules-mode").as_int();
        out.lle_modules.clear();
        for (const auto &m : core.child("lle-modules"))
            out.lle_modules.emplace_back(m.text().as_string());
    }

    if (!config_child.child("cpu").empty())
        out.cpu_opt = config_child.child("cpu").attribute("cpu-opt").as_bool();

    if (!config_child.child("gpu").empty()) {
        const auto gpu = config_child.child("gpu");
        out.backend_renderer = gpu.attribute("backend-renderer").as_string();
        out.gpu_idx = gpu.attribute("gpu-idx").as_int();
#ifdef __ANDROID__
        out.custom_driver_name = gpu.attribute("custom-driver-name").as_string();
#endif
        out.high_accuracy = gpu.attribute("high-accuracy").as_bool();
        out.resolution_multiplier = gpu.attribute("resolution-multiplier").as_float();
        out.disable_surface_sync = gpu.attribute("disable-surface-sync").as_bool();
        out.screen_filter = gpu.attribute("screen-filter").as_string();
        out.memory_mapping = gpu.attribute("memory-mapping").as_string();
        out.v_sync = gpu.attribute("v-sync").as_bool();
        out.anisotropic_filtering = gpu.attribute("anisotropic-filtering").as_int();
        out.async_pipeline_compilation = gpu.attribute("async-pipeline-compilation").as_bool();
        out.import_textures = gpu.attribute("import-textures").as_bool();
        out.export_textures = gpu.attribute("export-textures").as_bool();
        out.export_as_png = gpu.attribute("export-as-png").as_bool();
        out.fps_hack = gpu.attribute("fps-hack").as_bool();
        out.shader_cache = gpu.attribute("shader-cache").as_bool(true);
        out.spirv_shader = gpu.attribute("spirv-shader").as_bool();
        out.texture_cache = gpu.attribute("texture-cache").as_bool(true);
    }

    if (!config_child.child("audio").empty()) {
        const auto audio = config_child.child("audio");
        out.audio_backend = audio.attribute("audio-backend").as_string();
        out.audio_volume = audio.attribute("audio-volume").as_int();
        out.ngs_enable = audio.attribute("enable-ngs").as_bool();
    }

    if (!config_child.child("system").empty()) {
        const auto sys = config_child.child("system");
        out.pstv_mode = sys.attribute("pstv-mode").as_bool();
        out.sys_button = sys.attribute("sys-button").as_int(static_cast<int>(SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS));
        out.sys_lang = sys.attribute("sys-lang").as_int(static_cast<int>(SCE_SYSTEM_PARAM_LANG_ENGLISH_US));
        out.sys_date_format = sys.attribute("sys-date-format").as_int(static_cast<int>(SCE_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY));
        out.sys_time_format = sys.attribute("sys-time-format").as_int(static_cast<int>(SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR));
        out.ime_langs.clear();
        for (const auto &lang : sys.child("ime-langs"))
            out.ime_langs.push_back(std::stoull(lang.text().as_string()));
        if (out.ime_langs.empty())
            out.ime_langs.push_back(4);
    }

    if (!config_child.child("emulator").empty()) {
        const auto emu = config_child.child("emulator");
        out.file_loading_delay = emu.attribute("file-loading-delay").as_int();
        out.stretch_the_display_area = emu.attribute("stretch-the-display-area").as_bool();
        out.fullscreen_hd_res_pixel_perfect = emu.attribute("fullscreen-hd-res-pixel-perfect").as_bool();
    }

    if (!config_child.child("debug").empty()) {
        const auto dbg = config_child.child("debug");
        out.log_active_shaders = dbg.attribute("log-active-shaders").as_bool();
        out.log_uniforms = dbg.attribute("log-uniforms").as_bool();
        out.color_surface_debug = dbg.attribute("color-surface-debug").as_bool();
        out.validation_layer = dbg.attribute("validation-layer").as_bool(true);
    }

    if (!config_child.child("network").empty())
        out.psn_signed_in = config_child.child("network").attribute("psn-signed-in").as_bool();

    return true;
}

bool save_custom_config(const Config::CurrentConfig &cc, const fs::path &config_path, const std::string &app_path) {
    if (app_path.empty())
        return false;

    const auto dir = config_path / "config";
    fs::create_directories(dir);

    pugi::xml_document doc;
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "utf-8";

    auto config_child = doc.append_child("config");

    auto core_child = config_child.append_child("core");
    core_child.append_attribute("modules-mode") = cc.modules_mode;
    auto lle_child = core_child.append_child("lle-modules");
    for (const auto &m : cc.lle_modules)
        lle_child.append_child("module").append_child(pugi::node_pcdata).set_value(m.c_str());

    auto cpu_child = config_child.append_child("cpu");
    cpu_child.append_attribute("cpu-opt") = cc.cpu_opt;

    auto gpu_child = config_child.append_child("gpu");
    gpu_child.append_attribute("backend-renderer") = cc.backend_renderer.c_str();
    gpu_child.append_attribute("gpu-idx") = cc.gpu_idx;
#ifdef __ANDROID__
    gpu_child.append_attribute("custom-driver-name") = cc.custom_driver_name.c_str();
#endif
    gpu_child.append_attribute("high-accuracy") = cc.high_accuracy;
    gpu_child.append_attribute("resolution-multiplier") = cc.resolution_multiplier;
    gpu_child.append_attribute("disable-surface-sync") = cc.disable_surface_sync;
    gpu_child.append_attribute("screen-filter") = cc.screen_filter.c_str();
    gpu_child.append_attribute("memory-mapping") = cc.memory_mapping.c_str();
    gpu_child.append_attribute("v-sync") = cc.v_sync;
    gpu_child.append_attribute("anisotropic-filtering") = cc.anisotropic_filtering;
    gpu_child.append_attribute("async-pipeline-compilation") = cc.async_pipeline_compilation;
    gpu_child.append_attribute("import-textures") = cc.import_textures;
    gpu_child.append_attribute("export-textures") = cc.export_textures;
    gpu_child.append_attribute("export-as-png") = cc.export_as_png;
    gpu_child.append_attribute("fps-hack") = cc.fps_hack;
    gpu_child.append_attribute("shader-cache") = cc.shader_cache;
    gpu_child.append_attribute("spirv-shader") = cc.spirv_shader;
    gpu_child.append_attribute("texture-cache") = cc.texture_cache;

    auto audio_child = config_child.append_child("audio");
    audio_child.append_attribute("audio-backend") = cc.audio_backend.c_str();
    audio_child.append_attribute("audio-volume") = cc.audio_volume;
    audio_child.append_attribute("enable-ngs") = cc.ngs_enable;

    auto system_child = config_child.append_child("system");
    system_child.append_attribute("pstv-mode") = cc.pstv_mode;
    system_child.append_attribute("sys-button") = cc.sys_button;
    system_child.append_attribute("sys-lang") = cc.sys_lang;
    system_child.append_attribute("sys-date-format") = cc.sys_date_format;
    system_child.append_attribute("sys-time-format") = cc.sys_time_format;
    auto ime_child = system_child.append_child("ime-langs");
    for (const auto &lang : cc.ime_langs)
        ime_child.append_child("lang").append_child(pugi::node_pcdata).set_value(std::to_string(lang).c_str());

    auto emu_child = config_child.append_child("emulator");
    emu_child.append_attribute("file-loading-delay") = cc.file_loading_delay;
    emu_child.append_attribute("stretch-the-display-area") = cc.stretch_the_display_area;
    emu_child.append_attribute("fullscreen-hd-res-pixel-perfect") = cc.fullscreen_hd_res_pixel_perfect;

    auto debug_child = config_child.append_child("debug");
    debug_child.append_attribute("log-active-shaders") = cc.log_active_shaders;
    debug_child.append_attribute("log-uniforms") = cc.log_uniforms;
    debug_child.append_attribute("color-surface-debug") = cc.color_surface_debug;
    debug_child.append_attribute("validation-layer") = cc.validation_layer;

    auto network_child = config_child.append_child("network");
    network_child.append_attribute("psn-signed-in") = cc.psn_signed_in;

    const auto custom_cfg_path = get_custom_config_path(config_path, app_path);
    if (!doc.save_file(custom_cfg_path.c_str())) {
        LOG_ERROR("Failed to save custom config xml for app path: {}", app_path);
        return false;
    }

    return true;
}

bool delete_custom_config(const fs::path &config_path, const std::string &app_path) {
    if (app_path.empty())
        return false;

    const auto custom_cfg_path = get_custom_config_path(config_path, app_path);
    if (fs::exists(custom_cfg_path)) {
        fs::remove(custom_cfg_path);
        return true;
    }
    return false;
}

int delete_all_custom_configs(const fs::path &config_path) {
    const auto config_dir = config_path / "config";
    if (!fs::exists(config_dir) || !fs::is_directory(config_dir))
        return 0;

    int removed = 0;
    for (const auto &entry : fs::directory_iterator(config_dir)) {
        if (!entry.is_regular_file())
            continue;

        const auto file_name = entry.path().filename().string();
        if (!entry.path().has_extension() || entry.path().extension() != ".xml")
            continue;
        if (file_name.rfind("config_", 0) != 0)
            continue;

        boost::system::error_code error;
        if (fs::remove(entry.path(), error) && !error)
            ++removed;
    }

    return removed;
}

bool has_custom_config(const fs::path &config_path, const std::string &app_path) {
    if (app_path.empty())
        return false;
    return fs::exists(get_custom_config_path(config_path, app_path));
}

void set_current_config(Config &cfg, const fs::path &config_path, const std::string &app_path) {
    copy_global_to_current(cfg.current_config, cfg);
    if (!app_path.empty())
        load_custom_config(cfg.current_config, config_path, app_path);
}

void copy_current_config_to_global(Config &cfg) {
    copy_current_to_global(cfg, cfg.current_config);
}

void save_current_config(Config &cfg, const fs::path &config_path, const std::string &app_path, bool create_custom_if_missing) {
    if (!app_path.empty() && (create_custom_if_missing || has_custom_config(config_path, app_path))) {
        save_custom_config(cfg.current_config, config_path, app_path);
    } else {
        copy_current_config_to_global(cfg);
    }
    serialize_config(cfg, config_path);
}

std::vector<std::pair<std::string, bool>> get_modules_list(
    const fs::path &vita_fs_path,
    const std::vector<std::string> &lle_modules) {
    std::vector<std::pair<std::string, bool>> modules;

    const auto modules_path = vita_fs_path / "vs0/sys/external/";
    if (fs::exists(modules_path) && !fs::is_empty(modules_path)) {
        for (const auto &entry : fs::directory_iterator(modules_path)) {
            if (entry.path().extension() == ".suprx")
                modules.emplace_back(entry.path().filename().replace_extension().string(), false);
        }

        for (auto &m : modules)
            m.second = std::ranges::contains(lle_modules, m.first);

        std::sort(modules.begin(), modules.end(), [](const auto &a, const auto &b) {
            if (a.second == b.second)
                return a.first < b.first;
            return a.second;
        });
    }

    return modules;
}

} // namespace config
