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

#include "private.h"

#include <app/functions.h>
#include <audio/state.h>
#include <config/functions.h>
#include <config/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <host/dialog/filesystem.h>
#include <io/state.h>
#include <kernel/state.h>
#include <lang/functions.h>
#include <renderer/state.h>
#include <renderer/texture_cache.h>
#include <util/string_utils.h>

#include <gui/functions.h>
#include <gui/state.h>

#include <emuenv/state.h>

#include <string>
#include <util/fs.h>
#include <util/log.h>
#include <util/net_utils.h>

#include <SDL3/SDL_video.h>

#include <algorithm>
#include <pugixml.hpp>
#include <util/vector_utils.h>

#undef ERROR

namespace gui {

/**
 * @brief Struct used to abstract the code that manages app-specific config files
 *
 * This struct matches in type to the one found in `emuenv.cfg.current_config` and is
 * used to collect the proper values that must be set in `emuenv.cfg.current_config`
 * depending on whether an app-specific config file is being used or not.
 *
 * If an app-specific config file is loaded, then the values in this struct will be
 * set to those of the app-specific config file before getting used to set up `emuenv.cfg.current_config`
 * with those same values. If an app-specific config isn't loaded, then the values in this struct
 * will be set those of the global emulator settings before getting used to set up `emuenv.cfg.current_config`.
 */
static Config::CurrentConfig config;

void get_modules_list(GuiState &gui, EmuEnvState &emuenv) {
    gui.modules.clear();

    const auto modules_path{ emuenv.pref_path / "vs0/sys/external/" };
    if (fs::exists(modules_path) && !fs::is_empty(modules_path)) {
        for (const auto &module : fs::directory_iterator(modules_path)) {
            if (module.path().extension() == ".suprx")
                gui.modules.emplace_back(module.path().filename().replace_extension().string(), false);
        }

        for (auto &m : gui.modules)
            m.second = vector_utils::contains(config.lle_modules, m.first);

        std::sort(gui.modules.begin(), gui.modules.end(), [](const auto &ma, const auto &mb) {
            if (ma.second == mb.second)
                return ma.first < mb.first;
            return ma.second;
        });
    }
}

static void reset_emulator(GuiState &gui, EmuEnvState &emuenv) {
    gui.configuration_menu.settings_dialog = false;
    gui.vita_area.home_screen = false;

    // Clean and save new config value
    emuenv.cfg.auto_user_login = false;
    emuenv.cfg.user_id.clear();
    emuenv.cfg.set_pref_path(emuenv.pref_path);
    emuenv.io.user_id.clear();
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    // Clean User apps list
    gui.app_selector.user_apps.clear();

    get_modules_list(gui, emuenv);
    get_sys_apps_title(gui, emuenv);
    get_notice_list(emuenv);
    get_users_list(gui, emuenv);
    init_home(gui, emuenv);
}

static void change_emulator_path(GuiState &gui, EmuEnvState &emuenv) {
    fs::path emulator_path{};
    host::dialog::filesystem::Result result = host::dialog::filesystem::pick_folder(emulator_path);

    if (result == host::dialog::filesystem::Result::SUCCESS && emulator_path != emuenv.pref_path) {
        // Refresh the working paths
        emuenv.pref_path = emulator_path / "";

        // TODO: Move app old to new path
        reset_emulator(gui, emuenv);
        LOG_INFO("Successfully moved Vita3K path to: {}", emuenv.pref_path);
    }

    if (result == host::dialog::filesystem::Result::ERROR) {
        LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
    }
}

/**
 * @brief Set up `config` with the values contained in the custom config file of a certain PlayStation Vita application
 *
 * If a custom config is found, the configuration values found in the file will be assigned to
 * `config`.
 *
 * @param gui State of the Vita3K GUI
 * @param emuenv State of the emulated PlayStation Vita environment
 * @param app_path Path to the app or game to get the custom config for
 * @return true A custom config for the application has been found, and `config` has been set up with
 * the setting values contained in the custom config file.
 * @return false A custom config for the application has not been found or a custom config has been found
 * but it's corrupted or invalid.
 */
static bool get_custom_config(EmuEnvState &emuenv, const std::string &app_path) {
    const auto CUSTOM_CONFIG_PATH{ emuenv.config_path / "config" / fmt::format("config_{}.xml", app_path) };

    if (fs::exists(CUSTOM_CONFIG_PATH)) {
        pugi::xml_document custom_config_xml;
        if (custom_config_xml.load_file(CUSTOM_CONFIG_PATH.c_str()) && !custom_config_xml.child("config").empty()) {
            config = {};
            // Config
            const auto config_child = custom_config_xml.child("config");

            // Load Core Config
            if (!config_child.child("core").empty()) {
                const auto core_child = config_child.child("core");
                config.modules_mode = core_child.attribute("modules-mode").as_int();
                for (auto &m : core_child.child("lle-modules"))
                    config.lle_modules.emplace_back(m.text().as_string());
            }

            // Load CPU Config
            if (!config_child.child("cpu").empty()) {
                const auto cpu_child = config_child.child("cpu");
                config.cpu_opt = cpu_child.attribute("cpu-opt").as_bool();
            }

            // Load GPU Config
            if (!config_child.child("gpu").empty()) {
                const auto gpu_child = config_child.child("gpu");
                config.backend_renderer = gpu_child.attribute("backend-renderer").as_string();
#ifdef __ANDROID__
                config.custom_driver_name = gpu_child.attribute("custom-driver-name").as_string();
#endif
                config.high_accuracy = gpu_child.attribute("high-accuracy").as_bool();
                config.resolution_multiplier = gpu_child.attribute("resolution-multiplier").as_float();
                config.disable_surface_sync = gpu_child.attribute("disable-surface-sync").as_bool();
                config.screen_filter = gpu_child.attribute("screen-filter").as_string();
                config.memory_mapping = gpu_child.attribute("memory-mapping").as_string();
                config.v_sync = gpu_child.attribute("v-sync").as_bool();
                config.anisotropic_filtering = gpu_child.attribute("anisotropic-filtering").as_int();
                config.async_pipeline_compilation = gpu_child.attribute("async-pipeline-compilation").as_bool();
                config.import_textures = gpu_child.attribute("import-textures").as_bool();
                config.export_textures = gpu_child.attribute("export-textures").as_bool();
                config.export_as_png = gpu_child.attribute("export-as-png").as_bool();
                config.fps_hack = gpu_child.attribute("fps-hack").as_bool();
            }

            // Load Audio Config
            if (!config_child.child("audio").empty()) {
                const auto audio_child = config_child.child("audio");
                config.audio_volume = audio_child.attribute("audio-volume").as_int();
                config.ngs_enable = audio_child.attribute("enable-ngs").as_bool();
            }

            // Load System Config
            const auto system_child = config_child.child("system");
            if (!system_child.empty())
                config.pstv_mode = system_child.attribute("pstv-mode").as_bool();

            // Load Emulator Config
            if (!config_child.child("emulator").empty()) {
                const auto emulator_child = config_child.child("emulator");
                config.show_touchpad_cursor = emulator_child.attribute("show-touchpad-cursor").as_bool();
                config.file_loading_delay = emulator_child.attribute("file-loading-delay").as_int();
            }

            // Load Network Config
            if (!config_child.child("network").empty()) {
                const auto network_child = config_child.child("network");
                config.psn_signed_in = network_child.attribute("psn-signed-in").as_bool();
            }

            return true;
        } else {
            LOG_ERROR("Custom config XML found is corrupted or invalid in path: {}", CUSTOM_CONFIG_PATH);
            fs::remove(CUSTOM_CONFIG_PATH);
            return false;
        }
    }

    return false;
}

static void set_backend_renderer(EmuEnvState &emuenv, const std::string &backend_renderer) {
#ifndef __APPLE__
    if (string_utils::toupper(backend_renderer) == "OPENGL")
        emuenv.backend_renderer = renderer::Backend::OpenGL;
    else
        emuenv.backend_renderer = renderer::Backend::Vulkan;
#else
    emuenv.backend_renderer = renderer::Backend::Vulkan;
#endif
}

static int current_aniso_filter_log, max_aniso_filter_log, audio_backend_idx, current_user_lang;
static std::vector<std::string> list_user_lang;

/**
 * @brief Initialize the `config` struct with the values set in the global emulator config.
 *
 * @param gui State of the Vita3K GUI
 * @param emuenv State of the emulated PlayStation Vita environment
 * @param app_path Path to the app or game to get the custom config for
 */
void init_config(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    // If no app-specific config file is being used for the initialized application,
    // set up `config` with the values set in the global emulator configuration
    if (!get_custom_config(emuenv, app_path)) {
        config.cpu_opt = emuenv.cfg.cpu_opt;
        config.modules_mode = emuenv.cfg.modules_mode;
        config.lle_modules = emuenv.cfg.lle_modules;
        config.backend_renderer = emuenv.cfg.backend_renderer;
#ifdef __ANDROID__
        config.custom_driver_name = emuenv.cfg.custom_driver_name;
#endif
        config.high_accuracy = emuenv.cfg.high_accuracy;
        config.resolution_multiplier = emuenv.cfg.resolution_multiplier;
        config.disable_surface_sync = emuenv.cfg.disable_surface_sync;
        config.screen_filter = emuenv.cfg.screen_filter;
        config.memory_mapping = emuenv.cfg.memory_mapping;
        config.v_sync = emuenv.cfg.v_sync;
        config.anisotropic_filtering = emuenv.cfg.anisotropic_filtering;
        config.async_pipeline_compilation = emuenv.cfg.async_pipeline_compilation;
        config.import_textures = emuenv.cfg.import_textures;
        config.export_textures = emuenv.cfg.export_textures;
        config.export_as_png = emuenv.cfg.export_as_png;
        config.fps_hack = emuenv.cfg.fps_hack;
        config.audio_volume = emuenv.cfg.audio_volume;
        config.ngs_enable = emuenv.cfg.ngs_enable;
        config.pstv_mode = emuenv.cfg.pstv_mode;
        config.show_touchpad_cursor = emuenv.cfg.show_touchpad_cursor;
        config.file_loading_delay = emuenv.cfg.file_loading_delay;
        config.psn_signed_in = emuenv.cfg.psn_signed_in;
    }

    set_backend_renderer(emuenv, config.backend_renderer);

    list_user_lang.clear();
    const auto get_list_user_lang = [&](const fs::path &path) {
        for (const auto &lang : fs::directory_iterator(path / "lang/user")) {
            if (lang.path().extension() == ".xml") {
                const auto lang_file_name = lang.path().filename().replace_extension().string();
                list_user_lang.push_back(lang_file_name);
            }
        }
    };

#ifndef __ANDROID__
    get_list_user_lang(emuenv.static_assets_path);
    if (emuenv.static_assets_path != emuenv.shared_path)
        get_list_user_lang(emuenv.shared_path);
#else
    list_user_lang.insert(list_user_lang.end(), { "id", "ms", "ua" });
#endif

    current_user_lang = emuenv.cfg.user_lang.empty() ? 0 : (vector_utils::find_index(list_user_lang, emuenv.cfg.user_lang) + 1);

    get_modules_list(gui, emuenv);
    config.stretch_the_display_area = emuenv.cfg.stretch_the_display_area;
    config.fullscreen_hd_res_pixel_perfect = emuenv.cfg.fullscreen_hd_res_pixel_perfect;
    current_aniso_filter_log = static_cast<int>(log2f(static_cast<float>(config.anisotropic_filtering)));
    max_aniso_filter_log = static_cast<int>(log2f(static_cast<float>(emuenv.renderer->get_max_anisotropic_filtering())));
    audio_backend_idx = (emuenv.cfg.audio_backend == "SDL") ? 0 : 1;
    emuenv.app_path = app_path;
    emuenv.display.imgui_render = true;
}

/**
 * @brief Save settings to file
 *
 * The function serves to save settings for both app-specific custom settings files and
 * the global emulator settings file. The function automatically determines which file the
 * function has to work with by detecting the kind of settings dialog the user is using
 * from the GUI state.
 *
 * @param gui State of the Vita3K GUI
 * @param emuenv State of the emulated PlayStation Vita environment
 */
static void save_config(GuiState &gui, EmuEnvState &emuenv) {
    if (gui.configuration_menu.custom_settings_dialog) {
        const auto CONFIG_PATH{ emuenv.config_path / "config" };
        const auto CUSTOM_CONFIG_PATH{ CONFIG_PATH / fmt::format("config_{}.xml", emuenv.app_path) };
        fs::create_directories(CONFIG_PATH);

        pugi::xml_document custom_config_xml;
        auto declarationUser = custom_config_xml.append_child(pugi::node_declaration);
        declarationUser.append_attribute("version") = "1.0";
        declarationUser.append_attribute("encoding") = "utf-8";

        // Config
        auto config_child = custom_config_xml.append_child("config");

        // Core
        auto core_child = config_child.append_child("core");
        core_child.append_attribute("modules-mode") = config.modules_mode;
        auto enable_module = core_child.append_child("lle-modules");
        for (const auto &m : config.lle_modules)
            enable_module.append_child("module").append_child(pugi::node_pcdata).set_value(m.c_str());

        // CPU
        auto cpu_child = config_child.append_child("cpu");
        cpu_child.append_attribute("cpu-opt") = config.cpu_opt;

        // GPU
        auto gpu_child = config_child.append_child("gpu");
        gpu_child.append_attribute("backend-renderer") = config.backend_renderer.c_str();
#ifdef __ANDROID__
        gpu_child.append_attribute("custom-driver-name") = config.custom_driver_name.c_str();
#endif
        gpu_child.append_attribute("high-accuracy") = config.high_accuracy;
        gpu_child.append_attribute("resolution-multiplier") = config.resolution_multiplier;
        gpu_child.append_attribute("disable-surface-sync") = config.disable_surface_sync;
        gpu_child.append_attribute("screen-filter") = config.screen_filter.c_str();
        gpu_child.append_attribute("memory-mapping") = config.memory_mapping.c_str();
        gpu_child.append_attribute("v-sync") = config.v_sync;
        gpu_child.append_attribute("anisotropic-filtering") = config.anisotropic_filtering;
        gpu_child.append_attribute("async-pipeline-compilation") = config.async_pipeline_compilation;
        gpu_child.append_attribute("import-textures") = config.import_textures;
        gpu_child.append_attribute("export-textures") = config.export_textures;
        gpu_child.append_attribute("export-as-png") = config.export_as_png;
        gpu_child.append_attribute("fps-hack") = config.fps_hack;

        // Audio
        auto audio_child = config_child.append_child("audio");
        audio_child.append_attribute("audio-volume") = config.audio_volume;
        audio_child.append_attribute("enable-ngs") = config.ngs_enable;

        // System
        auto system_child = config_child.append_child("system");
        system_child.append_attribute("pstv-mode") = config.pstv_mode;

        // Emulator
        auto emulator_child = config_child.append_child("emulator");
        emulator_child.append_attribute("show-touchpad-cursor") = config.show_touchpad_cursor;
        emulator_child.append_attribute("file-loading-delay") = config.file_loading_delay;

        // Network
        auto network_child = config_child.append_child("network");
        network_child.append_attribute("psn-signed-in") = config.psn_signed_in;

        const auto save_xml = custom_config_xml.save_file(CUSTOM_CONFIG_PATH.c_str());
        if (!save_xml)
            LOG_ERROR("Failed to save custom config xml for app path: {}, in path: {}", emuenv.app_path, CONFIG_PATH);
    } else {
        emuenv.cfg.cpu_opt = config.cpu_opt;
        emuenv.cfg.modules_mode = config.modules_mode;
        emuenv.cfg.lle_modules = config.lle_modules;
        emuenv.cfg.backend_renderer = config.backend_renderer;
#ifdef __ANDROID__
        emuenv.cfg.custom_driver_name = config.custom_driver_name;
#endif
        emuenv.cfg.high_accuracy = config.high_accuracy;
        emuenv.cfg.resolution_multiplier = config.resolution_multiplier;
        emuenv.cfg.disable_surface_sync = config.disable_surface_sync;
        emuenv.cfg.screen_filter = config.screen_filter;
        emuenv.cfg.memory_mapping = config.memory_mapping;
        emuenv.cfg.v_sync = config.v_sync;
        emuenv.cfg.anisotropic_filtering = config.anisotropic_filtering;
        emuenv.cfg.async_pipeline_compilation = config.async_pipeline_compilation;
        emuenv.cfg.import_textures = config.import_textures;
        emuenv.cfg.export_textures = config.export_textures;
        emuenv.cfg.export_as_png = config.export_as_png;
        emuenv.cfg.fps_hack = config.fps_hack;
        emuenv.cfg.audio_volume = config.audio_volume;
        emuenv.cfg.ngs_enable = config.ngs_enable;
        emuenv.cfg.pstv_mode = config.pstv_mode;
        emuenv.cfg.show_touchpad_cursor = config.show_touchpad_cursor;
        emuenv.cfg.file_loading_delay = config.file_loading_delay;
        emuenv.cfg.psn_signed_in = config.psn_signed_in;
    }

    bool update_viewport_en = false;

    if (emuenv.cfg.fullscreen_hd_res_pixel_perfect != config.fullscreen_hd_res_pixel_perfect) {
        emuenv.cfg.fullscreen_hd_res_pixel_perfect = config.fullscreen_hd_res_pixel_perfect;
        update_viewport_en = true;
    }

    if (emuenv.cfg.stretch_the_display_area != config.stretch_the_display_area) {
        emuenv.cfg.stretch_the_display_area = config.stretch_the_display_area;
        update_viewport_en = true;
    }

    if (update_viewport_en)
        app::update_viewport(emuenv);

    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
}

static void set_vsync_state(const bool &state) {
    if (state) {
        // Try adaptive vsync first, falling back to regular vsync.
        if (!SDL_GL_SetSwapInterval(-1)) {
            SDL_GL_SetSwapInterval(1);
        }
    } else
        SDL_GL_SetSwapInterval(0);

    LOG_INFO("V-Sync state: {}", state);
}

void set_current_config(EmuEnvState &emuenv, const std::string &app_path) {
    // If a config file is in use, call `get_custom_config()` and set the config
    // parameters with the values stored in the app-specific custom config file
    if (!app_path.empty() && get_custom_config(emuenv, app_path))
        emuenv.cfg.current_config = config;
    else {
        // Else inherit the values from the global emulator config
        emuenv.cfg.current_config.cpu_opt = emuenv.cfg.cpu_opt;
        emuenv.cfg.current_config.modules_mode = emuenv.cfg.modules_mode;
        emuenv.cfg.current_config.lle_modules = emuenv.cfg.lle_modules;
        emuenv.cfg.current_config.backend_renderer = emuenv.cfg.backend_renderer;
#ifdef __ANDROID__
        emuenv.cfg.current_config.custom_driver_name = emuenv.cfg.custom_driver_name;
#endif
        emuenv.cfg.current_config.high_accuracy = emuenv.cfg.high_accuracy;
        emuenv.cfg.current_config.resolution_multiplier = emuenv.cfg.resolution_multiplier;
        emuenv.cfg.current_config.disable_surface_sync = emuenv.cfg.disable_surface_sync;
        emuenv.cfg.current_config.screen_filter = emuenv.cfg.screen_filter;
        emuenv.cfg.current_config.memory_mapping = emuenv.cfg.memory_mapping;
        emuenv.cfg.current_config.v_sync = emuenv.cfg.v_sync;
        emuenv.cfg.current_config.anisotropic_filtering = emuenv.cfg.anisotropic_filtering;
        emuenv.cfg.current_config.async_pipeline_compilation = emuenv.cfg.async_pipeline_compilation;
        emuenv.cfg.current_config.import_textures = emuenv.cfg.import_textures;
        emuenv.cfg.current_config.export_textures = emuenv.cfg.export_textures;
        emuenv.cfg.current_config.export_as_png = emuenv.cfg.export_as_png;
        emuenv.cfg.current_config.fps_hack = emuenv.cfg.fps_hack;
        emuenv.cfg.current_config.audio_volume = emuenv.cfg.audio_volume;
        emuenv.cfg.current_config.ngs_enable = emuenv.cfg.ngs_enable;
        emuenv.cfg.current_config.pstv_mode = emuenv.cfg.pstv_mode;
        emuenv.cfg.current_config.show_touchpad_cursor = emuenv.cfg.show_touchpad_cursor;
        emuenv.cfg.current_config.file_loading_delay = emuenv.cfg.file_loading_delay;
        emuenv.cfg.current_config.psn_signed_in = emuenv.cfg.psn_signed_in;
    }

    set_backend_renderer(emuenv, emuenv.cfg.current_config.backend_renderer);
}

/**
 * @brief Set up the config parameters on the emulated PlayStation Vita environment
 * that are susceptible to vary via app-specific config files with the proper values
 * depending on whether app-specific config files are being used or not.
 *
 * @param emuenv State of the emulated PlayStation Vita environment
 * @param app_path Path to the app or game to get the custom config for
 */
void set_config(EmuEnvState &emuenv) {
    set_current_config(emuenv, emuenv.io.app_path);

    // If backend render or resolution multiplier is changed when app run, reboot emu and app
    if (!emuenv.io.title_id.empty() && ((emuenv.renderer->current_backend != emuenv.backend_renderer)
#ifdef __ANDROID__
            || (emuenv.renderer->current_custom_driver != emuenv.cfg.current_config.custom_driver_name)
#endif
            || (emuenv.renderer->res_multiplier != emuenv.cfg.current_config.resolution_multiplier))) {
        emuenv.load_exec = true;
        emuenv.load_app_path = emuenv.io.app_path;
        emuenv.load_exec_path = emuenv.self_path;
        if (!emuenv.cfg.app_args.empty())
            emuenv.load_exec_argv = "\"" + emuenv.cfg.app_args + "\"";
        return;
    }

    // can be changed while ingame
    emuenv.renderer->set_surface_sync_state(emuenv.cfg.current_config.disable_surface_sync);
    emuenv.renderer->set_screen_filter(emuenv.cfg.current_config.screen_filter);
    if (emuenv.renderer->current_backend == renderer::Backend::OpenGL)
        set_vsync_state(emuenv.cfg.current_config.v_sync);
#ifdef __ANDROID__
    if (emuenv.renderer->support_custom_drivers())
        emuenv.renderer->set_turbo_mode(emuenv.cfg.turbo_mode);
#endif

    emuenv.renderer->res_multiplier = emuenv.cfg.current_config.resolution_multiplier;
    emuenv.renderer->set_anisotropic_filtering(emuenv.cfg.current_config.anisotropic_filtering);
    emuenv.renderer->set_stretch_display(emuenv.cfg.stretch_the_display_area);
    emuenv.renderer->stretch_hd_pixel_perfect(emuenv.cfg.fullscreen_hd_res_pixel_perfect);
    emuenv.renderer->get_texture_cache()->set_replacement_state(emuenv.cfg.current_config.import_textures, emuenv.cfg.current_config.export_textures, emuenv.cfg.current_config.export_as_png);
    emuenv.renderer->set_async_compilation(emuenv.cfg.current_config.async_pipeline_compilation);
    emuenv.display.fps_hack = emuenv.cfg.current_config.fps_hack;

    // No change it if app already running
    if (emuenv.io.title_id.empty())
        emuenv.kernel.cpu_opt = emuenv.cfg.current_config.cpu_opt;

    emuenv.audio.set_global_volume(emuenv.cfg.current_config.audio_volume / 100.f);
}

void draw_settings_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    auto &lang = gui.lang.settings_dialog;
    auto &common = emuenv.common_dialog.lang;
    auto &firmware_font = gui.lang.install_dialog.firmware_install;

    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2.f), emuenv.logical_viewport_pos.y + (display_size.y / 2.f)), ImGuiCond_Always, ImVec2(0.5f, 0.48f));
    const auto is_custom_config = gui.configuration_menu.custom_settings_dialog;
    // Reference here is intentional
    auto &show_settings_dialog = is_custom_config ? gui.configuration_menu.custom_settings_dialog : gui.configuration_menu.settings_dialog;
    ImGui::Begin("##settings", &show_settings_dialog, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(0.7f * RES_SCALE.x);
    const auto settings_str = lang.main_window["title"];
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, (is_custom_config ? fmt::format("{}: {} [{}]", settings_str, get_app_index(gui, emuenv.app_path)->title, emuenv.app_path) : settings_str).c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::BeginTabBar("SettingsTabBar", ImGuiTabBarFlags_None);

    // Core
    if (ImGui::BeginTabItem(lang.core["title"].c_str())) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (!gui.modules.empty()) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang.core["modules_mode"].c_str());
            ImGui::Spacing();
            ImGui::RadioButton(lang.core["automatic"].c_str(), &config.modules_mode, 0);
            SetTooltipEx(lang.core["automatic_description"].c_str());
            ImGui::SameLine();
            ImGui::RadioButton(lang.core["auto_manual"].c_str(), &config.modules_mode, 1);
            SetTooltipEx(lang.core["auto_manual_description"].c_str());
            ImGui::SameLine();
            ImGui::RadioButton(lang.core["manual"].c_str(), &config.modules_mode, 2);
            SetTooltipEx(lang.core["manual_description"].c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang.core["modules_list"].c_str());
            SetTooltipEx(lang.core["select_modules"].c_str());
            ImGui::Spacing();
            ImGui::PushItemWidth(260 * SCALE.x);
            if (ImGui::BeginListBox("##modules_list", { 0.0f, ImGui::GetTextLineHeightWithSpacing() * 8.25f + ImGui::GetStyle().FramePadding.y * 2.0f })) {
                for (auto &m : gui.modules) {
                    const auto module = std::find(config.lle_modules.begin(), config.lle_modules.end(), m.first);
                    const bool module_existed = (module != config.lle_modules.end());
                    if (!gui.module_search_bar.PassFilter(m.first.c_str()))
                        continue;
                    if (ImGui::Selectable(m.first.c_str(), &m.second, config.modules_mode == ModulesMode::AUTOMATIC ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
                        if (module_existed)
                            config.lle_modules.erase(module);
                        else
                            config.lle_modules.push_back(m.first);
                    }
                    ImGui::ScrollWhenDragging();
                }
                ImGui::EndListBox();
            }
            ImGui::PopItemWidth();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.core["search_modules"].c_str());
            gui.module_search_bar.Draw("##module_search_bar", 260 * SCALE.x);
            ImGui::Spacing();
            if (ImGui::Button(lang.core["clear_list"].c_str())) {
                config.lle_modules.clear();
                for (auto &m : gui.modules)
                    m.second = false;
            }
            ImGui::SameLine();
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.core["no_modules"].c_str());
            if (ImGui::Button(gui.lang.welcome["download_firmware"].c_str()))
                get_firmware_file(emuenv);
        }
        if (ImGui::Button(lang.core["refresh_list"].c_str()))
            get_modules_list(gui, emuenv);
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // CPU
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("CPU")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox(lang.cpu["cpu_opt"].c_str(), &config.cpu_opt);
        SetTooltipEx(lang.cpu["cpu_opt_description"].c_str());
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // GPU
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("GPU")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();

#ifdef __APPLE__
        ImGui::BeginDisabled();
#endif
        static const char *LIST_BACKEND_RENDERER[] = { "OpenGL", "Vulkan" };
        if (ImGui::Combo(lang.gpu["backend_renderer"].c_str(), reinterpret_cast<int *>(&emuenv.backend_renderer), LIST_BACKEND_RENDERER, IM_ARRAYSIZE(LIST_BACKEND_RENDERER)))
            config.backend_renderer = LIST_BACKEND_RENDERER[static_cast<int>(emuenv.backend_renderer)];
        SetTooltipEx(lang.gpu["select_backend_renderer"].c_str());
#ifdef __APPLE__
        ImGui::EndDisabled();
#else
        ImGui::Spacing();
#endif

        const bool is_vulkan = (emuenv.backend_renderer == renderer::Backend::Vulkan);
        const bool is_ingame = !emuenv.io.title_id.empty();
        const bool is_renderer_changed = (emuenv.backend_renderer != emuenv.renderer->current_backend);
        if (is_vulkan && !is_renderer_changed) {
            const std::vector<std::string> gpu_list_str = emuenv.renderer->get_gpu_list();
            // must convert to a vector of char*
            std::vector<const char *> gpu_list;
            for (const auto &gpu : gpu_list_str)
                gpu_list.push_back(gpu.c_str());
            ImGui::Combo(lang.gpu["gpu"].c_str(), &emuenv.cfg.gpu_idx, gpu_list.data(), static_cast<int>(gpu_list.size()));
            SetTooltipEx(lang.gpu["select_gpu"].c_str());

#ifdef __ANDROID__
            if (emuenv.renderer->support_custom_drivers()) {
                if (emuenv.cfg.gpu_idx == 0)
                    config.custom_driver_name = "";

                if (ImGui::Button(lang.gpu["add_custom_driver"].c_str())) {
                    app::add_custom_driver(emuenv);
                    // also set it to stock after
                    emuenv.cfg.gpu_idx = 0;
                }

                ImGui::SameLine();
                if (ImGui::Button("Download Custom Driver"))
                    open_path("https://github.com/K11MCH1/AdrenoToolsDrivers/releases/");

                // first is the stock gpu
                if (emuenv.cfg.gpu_idx > 0) {
                    config.custom_driver_name = gpu_list_str[emuenv.cfg.gpu_idx];

                    ImGui::SameLine();
                    if (ImGui::Button(lang.gpu["remove_custom_driver"].c_str())) {
                        app::remove_custom_driver(emuenv, config.custom_driver_name);
                        // set back to stock
                        emuenv.cfg.gpu_idx = 0;
                        config.custom_driver_name = "";
                    }
                }
            }
#endif

            if (is_ingame)
                ImGui::BeginDisabled();

            const char *LIST_RENDERER_ACCURACY[] = { lang.gpu["standard"].c_str(), lang.gpu["high"].c_str() };
            int is_high_accuracy = static_cast<int>(config.high_accuracy);
            ImGui::Combo(lang.gpu["renderer_accuracy"].c_str(), &is_high_accuracy, LIST_RENDERER_ACCURACY, IM_ARRAYSIZE(LIST_RENDERER_ACCURACY));
            config.high_accuracy = static_cast<bool>(is_high_accuracy);

            if (is_ingame)
                ImGui::EndDisabled();
        } else if (!is_vulkan) {
            ImGui::Checkbox(lang.gpu["v_sync"].c_str(), &config.v_sync);
            SetTooltipEx(lang.gpu["v_sync_description"].c_str());
            ImGui::SameLine();
        }
        bool has_surface_sync = !is_vulkan || (emuenv.renderer->supported_mapping_methods_mask > 1);
#ifdef __ANDROID__
        has_surface_sync &= is_vulkan;
#endif

        const bool has_integer_multiplier = static_cast<int>(config.resolution_multiplier * 4.0f) % 4 == 0;
        // OpenGL does not support surface sync with a non-integer resolution multiplier
        if (!is_vulkan && !has_integer_multiplier)
            has_surface_sync = false;

        if (!has_surface_sync)
            config.disable_surface_sync = true;

        if (has_surface_sync) {
            // surface sync is supported on vulkan only when memory mapping is enabled
            ImGui::Checkbox(lang.gpu["disable_surface_sync"].c_str(), &config.disable_surface_sync);
            SetTooltipEx(lang.gpu["surface_sync_description"].c_str());

            if (is_vulkan)
                ImGui::SameLine();
        }

        if (is_vulkan) {
            ImGui::Checkbox(lang.gpu["async_pipeline_compilation"].c_str(), &config.async_pipeline_compilation);
            SetTooltipEx(lang.gpu["async_pipeline_compilation_description"].c_str());
        }

        // Screen Filter
        ImGui::Spacing();
        int curr_filter = 0;
        const std::array<const char *, 5> possible_filters = {
            lang.gpu["nearest"].c_str(),
            lang.gpu["bilinear"].c_str(),
            lang.gpu["bicubic"].c_str(),
            "FXAA",
            "FSR"
        };
        const int filters_available = emuenv.renderer->get_supported_filters();
        std::vector<const char *> filters;
        for (int i = 0; i < possible_filters.size(); i++) {
            if (config.screen_filter == possible_filters[i])
                curr_filter = filters.size();

            if ((1 << i) & filters_available)
                filters.push_back(possible_filters[i]);
        }

        if (ImGui::Combo(lang.gpu["screen_filter"].c_str(), &curr_filter, filters.data(), filters.size()))
            config.screen_filter = filters[curr_filter];
        SetTooltipEx(lang.gpu["screen_filter_description"].c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Resolution Upscaling
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.gpu["internal_resolution_upscaling"].c_str());
        ImGui::Spacing();
        ImGui::PushID("Res scal");
        if (config.resolution_multiplier == 0.5f)
            ImGui::BeginDisabled();
        if (ImGui::Button("<", ImVec2(20.f * SCALE.x, 0)))
            config.resolution_multiplier -= 0.25f;
        if (config.resolution_multiplier == 0.5f)
            ImGui::EndDisabled();
        ImGui::SameLine(0, 5.f * SCALE.x);
        ImGui::PushItemWidth(-100.f * SCALE.x);
        int slider_position = static_cast<int>(config.resolution_multiplier * 4);
        if (ImGui::SliderInt("##res_scal", &slider_position, 2, 32, fmt::format("{}x", config.resolution_multiplier).c_str(), ImGuiSliderFlags_None)) {
            config.resolution_multiplier = static_cast<float>(slider_position) / 4.0f;
            if (config.resolution_multiplier != 1.0f && !is_vulkan)
                config.disable_surface_sync = true;
        }
        ImGui::PopItemWidth();
        SetTooltipEx(lang.gpu["internal_resolution_upscaling_description"].c_str());
        ImGui::SameLine(0, 5 * SCALE.x);
        if (config.resolution_multiplier == 8.0f)
            ImGui::BeginDisabled();
        if (ImGui::Button(">", ImVec2(20.f * SCALE.x, 0)))
            config.resolution_multiplier += 0.25f;
        if (config.resolution_multiplier == 8.0f)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if ((config.resolution_multiplier == 1.0f) && !config.disable_surface_sync)
            ImGui::BeginDisabled();
        if (ImGui::Button(lang.gpu["reset"].c_str(), ImVec2(60.f * SCALE.x, 0)))
            config.resolution_multiplier = 1.0f;

        if ((config.resolution_multiplier == 1.0f) && !config.disable_surface_sync)
            ImGui::EndDisabled();
        ImGui::Spacing();
        const auto res_scal = fmt::format("{}x{}", static_cast<int>(960 * config.resolution_multiplier), static_cast<int>(544 * config.resolution_multiplier));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(res_scal.c_str()).x / 2.f) - (35.f * SCALE.x));
        ImGui::Text("%s", res_scal.c_str());
        ImGui::PopID();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Anisotropic Filtering
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.gpu["anisotropic_filtering"].c_str());
        ImGui::Spacing();
        ImGui::PushID("Aniso filter");
        if (config.anisotropic_filtering == 1)
            ImGui::BeginDisabled();
        if (ImGui::Button("<", ImVec2(20.f * SCALE.x, 0)))
            config.anisotropic_filtering = 1 << --current_aniso_filter_log;
        if (config.anisotropic_filtering == 1)
            ImGui::EndDisabled();
        ImGui::SameLine(0, 5 * SCALE.x);
        ImGui::PushItemWidth(-100.f * SCALE.x);
        if (ImGui::SliderInt("##aniso_filter", &current_aniso_filter_log, 0, max_aniso_filter_log, fmt::format("{}x", config.anisotropic_filtering).c_str()))
            config.anisotropic_filtering = 1 << current_aniso_filter_log;
        ImGui::PopItemWidth();
        SetTooltipEx(lang.gpu["anisotropic_filtering_description"].c_str());
        ImGui::SameLine(0, 5 * SCALE.x);
        if (current_aniso_filter_log == max_aniso_filter_log)
            ImGui::BeginDisabled();
        if (ImGui::Button(">", ImVec2(20.f * SCALE.x, 0)))
            config.anisotropic_filtering = 1 << ++current_aniso_filter_log;
        if (current_aniso_filter_log == max_aniso_filter_log)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (config.anisotropic_filtering == 1)
            ImGui::BeginDisabled();
        if (ImGui::Button(lang.gpu["reset"].c_str(), ImVec2(60.f * SCALE.x, 0))) {
            config.anisotropic_filtering = 1;
            current_aniso_filter_log = 0;
        }
        if (config.anisotropic_filtering == 1)
            ImGui::EndDisabled();
        ImGui::PopID();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Texture Replacement
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.gpu["texture_replacement"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.gpu["export_textures"].c_str(), &config.export_textures);
        ImGui::SameLine();
        ImGui::Checkbox(lang.gpu["import_textures"].c_str(), &config.import_textures);

        static const char *export_formats[] = { "PNG", "DDS" };
        int export_format_pos = config.export_as_png ? 0 : 1;
        if (ImGui::Combo(lang.gpu["texture_exporting_format"].c_str(), &export_format_pos, export_formats, IM_ARRAYSIZE(export_formats)))
            config.export_as_png = export_format_pos == 0;

        // FPS hack
        ImGui::Checkbox(lang.gpu["fps_hack"].c_str(), &config.fps_hack);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", lang.gpu["fps_hack_description"].c_str());
        }

        if (emuenv.renderer->supported_mapping_methods_mask > 1 && !is_renderer_changed) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (is_ingame)
                ImGui::BeginDisabled();

            std::vector<const char *> mapping_methods_strings = {
                lang.gpu["disabled"].c_str(),
                lang.gpu["double_buffer"].c_str(),
                lang.gpu["external_host"].c_str(),
                lang.gpu["page_table"].c_str(),
                lang.gpu["native_buffer"].c_str()
            };
            std::vector<std::string_view> mapping_methods_indexes = {
                "disabled",
                "double-buffer",
                "external-host",
                "page-table",
                "native-buffer"
            };

            // only get the mapping methods that are available on this GPU
            int list_pos = 0;
            for (int i = 0; i < 5; i++) {
                if ((1 << i) & emuenv.renderer->supported_mapping_methods_mask) {
                    list_pos++;
                } else {
                    mapping_methods_strings.erase(mapping_methods_strings.begin() + list_pos);
                    mapping_methods_indexes.erase(mapping_methods_indexes.begin() + list_pos);
                }
            }

            static int current_mapping = std::find(mapping_methods_indexes.begin(), mapping_methods_indexes.end(), config.memory_mapping) - mapping_methods_indexes.begin();
            if (ImGui::Combo(lang.gpu["mapping_method"].c_str(), &current_mapping, mapping_methods_strings.data(), mapping_methods_strings.size())) {
                config.memory_mapping = mapping_methods_indexes[current_mapping];
            }
            SetTooltipEx(lang.gpu["mapping_method_description"].c_str());

            if (is_ingame)
                ImGui::EndDisabled();
        }

#ifdef __ANDROID__
        if (emuenv.renderer->support_custom_drivers()) {
            ImGui::Spacing();
            ImGui::Checkbox(lang.gpu["turbo_mode"].c_str(), &emuenv.cfg.turbo_mode);
            SetTooltipEx(lang.gpu["turbo_mode_description"].c_str());
        }
#endif

        // Shaders
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.gpu["shaders"].c_str());
        ImGui::Spacing();
        if (!is_vulkan) {
            ImGui::Checkbox(lang.gpu["shader_cache"].c_str(), &emuenv.cfg.shader_cache);
            SetTooltipEx(lang.gpu["shader_cache_description"].c_str());
        }
        if (emuenv.renderer->features.spirv_shader) {
            ImGui::SameLine();
            ImGui::Checkbox(lang.gpu["spirv_shader"].c_str(), &emuenv.cfg.spirv_shader);
            SetTooltipEx(lang.gpu["spirv_shader_description"].c_str());
        }
        const auto shaders_cache_path{ emuenv.cache_path / "shaders" };
        if (fs::exists(shaders_cache_path) && !fs::is_empty(shaders_cache_path)) {
            ImGui::Spacing();
            if (ImGui::Button(lang.gpu["clean_shaders"].c_str())) {
                fs::remove_all(shaders_cache_path);
                fs::remove_all(emuenv.cache_path / "shaderlog");
                fs::remove_all(emuenv.log_path / "shaderlog");
            }
        }

        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Audio
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem(lang.audio["title"].c_str())) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (!emuenv.io.app_path.empty())
            ImGui::BeginDisabled();
        static const char *LIST_BACKEND_AUDIO[] = { "SDL", "Cubeb" };
        if (ImGui::Combo(lang.audio["audio_backend"].c_str(), &audio_backend_idx, LIST_BACKEND_AUDIO, IM_ARRAYSIZE(LIST_BACKEND_AUDIO)))
            emuenv.cfg.audio_backend = LIST_BACKEND_AUDIO[audio_backend_idx];
        SetTooltipEx(lang.audio["select_audio_backend"].c_str());
        if (!emuenv.io.app_path.empty())
            ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::SliderInt(lang.audio["audio_volume"].c_str(), &config.audio_volume, 0, 100, "%d %%", ImGuiSliderFlags_AlwaysClamp);
        SetTooltipEx(lang.audio["audio_volume_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.audio["enable_ngs_support"].c_str(), &config.ngs_enable);
        SetTooltipEx(lang.audio["ngs_description"].c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // System
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem(lang.system["title"].c_str())) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.system["select_enter_button"].c_str());
        SetTooltipEx(lang.system["enter_button_description"].c_str());
        ImGui::RadioButton(lang.system["circle"].c_str(), &emuenv.cfg.sys_button, 0);
        ImGui::RadioButton(lang.system["cross"].c_str(), &emuenv.cfg.sys_button, 1);
        ImGui::Spacing();
        ImGui::Checkbox(lang.system["pstv_mode"].c_str(), &config.pstv_mode);
        SetTooltipEx(lang.system["pstv_mode_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.system["show_mode"].c_str(), &emuenv.cfg.show_mode);
        SetTooltipEx(lang.system["show_mode_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.system["demo_mode"].c_str(), &emuenv.cfg.demo_mode);
        SetTooltipEx(lang.system["demo_mode_description"].c_str());
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Emulator
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem(lang.emulator["title"].c_str())) {
        ImGui::PopStyleColor();
#ifndef __ANDROID__
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["boot_apps_full_screen"].c_str(), &emuenv.cfg.boot_apps_full_screen);
#endif
        ImGui::Spacing();

        const char *LIST_LOG_LEVEL[] = { lang.emulator["trace"].c_str(), gui.lang.main_menubar.debug["title"].c_str(), lang.emulator["info"].c_str(), lang.emulator["warning"].c_str(), lang.emulator["error"].c_str(), lang.emulator["critical"].c_str(), lang.emulator["off"].c_str() };
        if (ImGui::Combo(lang.emulator["log_level"].c_str(), &emuenv.cfg.log_level, LIST_LOG_LEVEL, IM_ARRAYSIZE(LIST_LOG_LEVEL)))
            logging::set_level(static_cast<spdlog::level::level_enum>(emuenv.cfg.log_level));
        SetTooltipEx(lang.emulator["select_log_level"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["archive_log"].c_str(), &emuenv.cfg.archive_log);
        SetTooltipEx(lang.emulator["archive_log_description"].c_str());
        ImGui::SameLine();
#ifdef USE_DISCORD
        ImGui::Checkbox("Discord Rich Presence", &emuenv.cfg.discord_rich_presence);
        SetTooltipEx(lang.emulator["discord_rich_presence"].c_str());
#endif
        ImGui::Checkbox(lang.emulator["texture_cache"].c_str(), &emuenv.cfg.texture_cache);
        SetTooltipEx(lang.emulator["texture_cache_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.emulator["show_compile_shaders"].c_str(), &emuenv.cfg.show_compile_shaders);
        SetTooltipEx(lang.emulator["compile_shaders_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["show_touchpad_cursor"].c_str(), &config.show_touchpad_cursor);
        SetTooltipEx(lang.emulator["touchpad_cursor_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.emulator["log_compat_warn"].c_str(), &emuenv.cfg.log_compat_warn);
        SetTooltipEx(lang.emulator["log_compat_warn_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["check_for_updates"].c_str(), &emuenv.cfg.check_for_updates);
        SetTooltipEx(lang.emulator["check_for_updates_description"].c_str());
        ImGui::Spacing();
        ImGui::SliderInt(lang.emulator["file_loading_delay"].c_str(), &config.file_loading_delay, 0, 30, "%d ms", ImGuiSliderFlags_AlwaysClamp);
        SetTooltipEx(lang.emulator["file_loading_delay_description"].c_str());
        ImGui::Separator();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.emulator["performance_overlay"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.emulator["performance_overlay"].c_str(), &emuenv.cfg.performance_overlay);
        SetTooltipEx(lang.emulator["performance_overlay_description"].c_str());
        if (emuenv.cfg.performance_overlay) {
            const char *LIST_OVERLAY_DETAIL[] = { lang.emulator["minimum"].c_str(), lang.emulator["low"].c_str(), lang.emulator["medium"].c_str(), lang.emulator["maximum"].c_str() };
            ImGui::Combo(lang.emulator["detail"].c_str(), &emuenv.cfg.performance_overlay_detail, LIST_OVERLAY_DETAIL, IM_ARRAYSIZE(LIST_OVERLAY_DETAIL));
            SetTooltipEx(lang.emulator["select_detail"].c_str());
            const char *LIST_OVERLAY_POSITION[] = { lang.emulator["top_left"].c_str(), lang.emulator["top_center"].c_str(), lang.emulator["top_right"].c_str(), lang.emulator["bottom_left"].c_str(), lang.emulator["bottom_center"].c_str(), lang.emulator["bottom_right"].c_str() };
            ImGui::Combo(lang.emulator["position"].c_str(), &emuenv.cfg.performance_overlay_position, LIST_OVERLAY_POSITION, IM_ARRAYSIZE(LIST_OVERLAY_POSITION));
            SetTooltipEx(lang.emulator["select_position"].c_str());
        }
        ImGui::Spacing();
#ifndef _WIN32
        ImGui::Checkbox(lang.emulator["case_insensitive"].c_str(), &emuenv.io.case_isens_find_enabled);
        SetTooltipEx(lang.emulator["case_insensitive_description"].c_str());
#endif
        ImGui::Separator();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.emulator["emu_storage_folder"].c_str());
        ImGui::Spacing();
        ImGui::PushItemWidth(320);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang.emulator["current_emu_path"].c_str(), emuenv.cfg.pref_path.c_str());
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (ImGui::Button(lang.emulator["change_emu_path"].c_str()))
            change_emulator_path(gui, emuenv);
        SetTooltipEx(lang.emulator["change_emu_path_description"].c_str());
        if (emuenv.cfg.pref_path != emuenv.default_path) {
            ImGui::SameLine();
            if (ImGui::Button(lang.emulator["reset_emu_path"].c_str())) {
                if (emuenv.default_path != emuenv.pref_path) {
                    emuenv.pref_path = emuenv.default_path;

                    // Refresh the working paths
                    reset_emulator(gui, emuenv);
                    LOG_INFO("Successfully restored default path for Vita3K files to: {}", emuenv.pref_path);
                }
            }
            SetTooltipEx(lang.emulator["reset_emu_path_description"].c_str());
        }
        ImGui::Spacing();
#ifdef __ANDROID__
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.emulator["storage_folder_permissions"].c_str());
        ImGui::Spacing();
#endif
        ImGui::Separator();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.emulator["custom_config_settings"].c_str());
        ImGui::Spacing();
        if (ImGui::Button(lang.emulator["clear_custom_config"].c_str())) {
            if (fs::remove_all(emuenv.config_path / "config")) {
                LOG_INFO("Clear all custom config settings successfully.");
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang.emulator["screenshot_image_type"].c_str());
        ImGui::Spacing();
        const char *LIST_IMG_FORMAT[] = { lang.emulator["null"].c_str(), "JPEG", "PNG" };
        ImGui::Combo(lang.emulator["screenshot_format"].c_str(), &emuenv.cfg.screenshot_format, LIST_IMG_FORMAT, IM_ARRAYSIZE(LIST_IMG_FORMAT));
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // GUI
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem(lang.gui["title"].c_str())) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox(lang.gui["show_gui"].c_str(), &emuenv.cfg.show_gui);
        SetTooltipEx(lang.gui["gui_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.gui["show_info_bar"].c_str(), &emuenv.cfg.show_info_bar);
        SetTooltipEx(lang.gui["info_bar_description"].c_str());
        ImGui::Spacing();
        const std::string system_lang_name = fmt::format("{}: {}", lang.system["title"], get_sys_lang_name(emuenv.cfg.sys_lang));
        std::vector<const char *> list_user_lang_str{ system_lang_name.c_str() };
        static std::map<std::string, std::string> static_list_user_lang_names = {
            { "id", "Indonesia" },
            { "ms", "Malaysia" },
            { "ua", reinterpret_cast<const char *>(u8"Українська") },
        };
        for (const auto &l : list_user_lang)
            list_user_lang_str.push_back(static_list_user_lang_names.contains(l) ? static_list_user_lang_names[l].c_str() : l.c_str());
        if (ImGui::Combo(lang.gui["user_lang"].c_str(), &current_user_lang, list_user_lang_str.data(), static_cast<int>(list_user_lang_str.size()), 4)) {
            if (current_user_lang != 0)
                emuenv.cfg.user_lang = list_user_lang[current_user_lang - 1];
            else
                emuenv.cfg.user_lang.clear();

            lang::init_lang(gui.lang, emuenv);
        }
        SetTooltipEx(lang.gui["select_user_lang"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.gui["display_info_message"].c_str(), &emuenv.cfg.display_info_message);
        SetTooltipEx(lang.gui["display_info_message_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.gui["display_system_apps"].c_str(), &emuenv.cfg.display_system_apps);
        SetTooltipEx(lang.gui["display_system_apps_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.gui["show_live_area_screen"].c_str(), &emuenv.cfg.show_live_area_screen);
        SetTooltipEx(lang.gui["live_area_screen_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.gui["stretch_the_display_area"].c_str(), &config.stretch_the_display_area);
        SetTooltipEx(lang.gui["stretch_the_display_area_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.gui["apps_list_grid"].c_str(), &emuenv.cfg.apps_list_grid);
        SetTooltipEx(lang.gui["apps_list_grid_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.gui["fullscreen_hd_res_pixel_perfect"].c_str(), &config.fullscreen_hd_res_pixel_perfect);
        SetTooltipEx(lang.gui["fullscreen_hd_res_pixel_perfect_description"].c_str());
        if (!emuenv.cfg.apps_list_grid) {
            ImGui::Spacing();
            ImGui::SliderInt(lang.gui["icon_size"].c_str(), &emuenv.cfg.icon_size, 64, 128);
            SetTooltipEx(lang.gui["select_icon_size"].c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, lang.gui["font_support"].c_str());
        ImGui::Spacing();
        if (gui.fw_font) {
            ImGui::Checkbox(lang.gui["asia_font_support"].c_str(), &emuenv.cfg.asia_font_support);
            SetTooltipEx(lang.gui["asia_font_support_description"].c_str());
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", firmware_font["no_font_exist"].c_str());
            if (ImGui::Button(gui.lang.welcome["download_firmware_font_package"].c_str()))
                open_path("https://bit.ly/2P2rb0r");
            SetTooltipEx(lang.gui["firmware_font_package_description"].c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, lang.gui["theme_background"].c_str());
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang.gui["current_theme_content_id"].c_str(), gui.users[emuenv.io.user_id].theme_id.c_str());
        if (gui.users[emuenv.io.user_id].theme_id != "default") {
            ImGui::Spacing();
            if (ImGui::Button(lang.gui["reset_default_theme"].c_str())) {
                gui.users[emuenv.io.user_id].theme_id = "default";
                gui.users[emuenv.io.user_id].use_theme_bg = false;
                if (init_theme(gui, emuenv, "default"))
                    gui.users[emuenv.io.user_id].use_theme_bg = true;
                gui.users[emuenv.io.user_id].start_path.clear();
                gui.users[emuenv.io.user_id].start_type = "default";
                save_user(gui, emuenv, emuenv.io.user_id);
                init_theme_start_background(gui, emuenv, "default");
                init_apps_icon(gui, emuenv, gui.app_selector.sys_apps);
            }
            ImGui::SameLine();
        }
        if (!gui.theme_backgrounds.empty())
            if (ImGui::Checkbox(lang.gui["using_theme_background"].c_str(), &gui.users[emuenv.io.user_id].use_theme_bg))
                save_user(gui, emuenv, emuenv.io.user_id);

        if (!gui.user_backgrounds.empty()) {
            ImGui::Spacing();
            if (ImGui::Button(lang.gui["clean_user_backgrounds"].c_str())) {
                gui.user_backgrounds[gui.users[emuenv.io.user_id].backgrounds[gui.current_user_bg]] = {};
                gui.user_backgrounds.clear();
                if (!gui.theme_backgrounds.empty())
                    gui.users[emuenv.io.user_id].use_theme_bg = true;
                gui.users[emuenv.io.user_id].backgrounds.clear();
                save_user(gui, emuenv, emuenv.io.user_id);
            }
        }
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang.gui["current_start_background"].c_str(), gui.users[emuenv.io.user_id].start_type.c_str());
        if (((gui.users[emuenv.io.user_id].theme_id == "default") && (gui.users[emuenv.io.user_id].start_type != "default")) || ((gui.users[emuenv.io.user_id].theme_id != "default") && (gui.users[emuenv.io.user_id].start_type != "theme"))) {
            ImGui::Spacing();
            if (ImGui::Button(lang.gui["reset_start_background"].c_str())) {
                gui.users[emuenv.io.user_id].start_path.clear();
                init_theme_start_background(gui, emuenv, gui.users[emuenv.io.user_id].theme_id);
                gui.users[emuenv.io.user_id].start_type = (gui.users[emuenv.io.user_id].theme_id == "default") ? "default" : "theme";
                save_user(gui, emuenv, emuenv.io.user_id);
            }
        }
        if (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty()) {
            ImGui::Spacing();
            ImGui::SliderFloat(lang.gui["background_alpha"].c_str(), &emuenv.cfg.background_alpha, 0.999f, 0.000f);
            SetTooltipEx(lang.gui["select_background_alpha"].c_str());
        }
        if (!gui.theme_backgrounds.empty() || (gui.user_backgrounds.size() > 1)) {
            ImGui::Spacing();
            ImGui::SliderInt(lang.gui["delay_background"].c_str(), &emuenv.cfg.delay_background, 4, 60);
            SetTooltipEx(lang.gui["select_delay_background"].c_str());
        }
        ImGui::Spacing();
        ImGui::SliderInt(lang.gui["delay_start"].c_str(), &emuenv.cfg.delay_start, 10, 60);
        SetTooltipEx(lang.gui["select_delay_start"].c_str());
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Network
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem(lang.network["title"].c_str())) {
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // PSN
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, "PlayStation Network");
        ImGui::Spacing();
        ImGui::Checkbox(lang.network["psn_signed_in"].c_str(), &config.psn_signed_in);
        SetTooltipEx(lang.network["psn_signed_in_description"].c_str());

        // Adhoc
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, "Adhoc");
        ImGui::Spacing();

        std::vector<std::string> addrsStrings;
        std::vector<const char *> addrsSelect;
        std::vector<const char *> nMaskSelect;
        const auto addrs = net_utils::get_all_assigned_addrs();

        for (const auto &addr : addrs) {
            addrsStrings.emplace_back(fmt::format("{} ({})", addr.addr, addr.name).c_str());
            addrsSelect.emplace_back(addrsStrings.back().c_str());
            nMaskSelect.emplace_back(addr.netMask.c_str());
        }

        if (emuenv.cfg.adhoc_addr >= addrs.size()) {
            emuenv.cfg.adhoc_addr = 0;
            LOG_WARN("Invalid adhoc address index {}, resetting to 0", emuenv.cfg.adhoc_addr);
            save_config(gui, emuenv);
        }

        ImGui::PushItemWidth(ImGui::CalcTextSize(addrsStrings[emuenv.cfg.adhoc_addr].c_str()).x + (30.f * SCALE.x));
        ImGui::Combo(lang.network["ip_address"].c_str(), &emuenv.cfg.adhoc_addr, addrsSelect.data(), static_cast<int>(addrsSelect.size()));
        SetTooltipEx(lang.network["ip_address_description"].c_str());

        ImGui::BeginDisabled();
        ImGui::Combo(lang.network["subnet_mask"].c_str(), &emuenv.cfg.adhoc_addr, nMaskSelect.data(), static_cast<int>(nMaskSelect.size()));
        ImGui::EndDisabled();
        ImGui::PopItemWidth();

        // HTTP
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, "HTTP");
        ImGui::Spacing();
        ImGui::Checkbox(lang.network["enable_http"].c_str(), &emuenv.cfg.http_enable);
        SetTooltipEx(lang.network["enable_http_description"].c_str());
        ImGui::Spacing();
        ImGui::SliderInt(lang.network["timeout_attempts"].c_str(), &emuenv.cfg.http_timeout_attempts, 0, 100);
        SetTooltipEx(lang.network["timeout_attempts_description"].c_str());
        ImGui::SliderInt(lang.network["timeout_sleep"].c_str(), &emuenv.cfg.http_timeout_sleep_ms, 50, 3000);
        SetTooltipEx(lang.network["timeout_sleep_description"].c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SliderInt(lang.network["read_end_attempts"].c_str(), &emuenv.cfg.http_read_end_attempts, 0, 100);
        SetTooltipEx(lang.network["read_end_attempts_description"].c_str());
        ImGui::SliderInt(lang.network["read_end_sleep"].c_str(), &emuenv.cfg.http_read_end_sleep_ms, 50, 3000);
        SetTooltipEx(lang.network["read_end_sleep_description"].c_str());
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Debug
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem(gui.lang.main_menubar.debug["title"].c_str())) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox(lang.debug["log_imports"].c_str(), &emuenv.kernel.debugger.log_imports);
        ImGui::SameLine();
        SetTooltipEx(lang.debug["log_imports_description"].c_str());
        ImGui::Checkbox(lang.debug["log_exports"].c_str(), &emuenv.kernel.debugger.log_exports);
        SetTooltipEx(lang.debug["log_exports_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.debug["log_active_shaders"].c_str(), &emuenv.cfg.log_active_shaders);
        SetTooltipEx(lang.debug["log_active_shaders_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang.debug["log_uniforms"].c_str(), &emuenv.cfg.log_uniforms);
        ImGui::SameLine();
        SetTooltipEx(lang.debug["log_uniforms_description"].c_str());
        ImGui::Checkbox(lang.debug["color_surface_debug"].c_str(), &emuenv.cfg.color_surface_debug);
        SetTooltipEx(lang.debug["color_surface_debug_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang.debug["dump_elfs"].c_str(), &emuenv.kernel.debugger.dump_elfs);
        SetTooltipEx(lang.debug["dump_elfs_description"].c_str());
        if (emuenv.backend_renderer == renderer::Backend::Vulkan) {
            ImGui::Spacing();
            ImGui::Checkbox(lang.debug["validation_layer"].c_str(), &emuenv.cfg.validation_layer);
            ImGui::SameLine();
            SetTooltipEx(lang.debug["validation_layer_description"].c_str());
        }
        ImGui::Spacing();
        if (ImGui::Button(emuenv.kernel.debugger.watch_code ? lang.debug["unwatch_code"].c_str() : lang.debug["watch_code"].c_str())) {
            emuenv.kernel.debugger.watch_code = !emuenv.kernel.debugger.watch_code;
            emuenv.kernel.debugger.update_watches();
        }
        ImGui::SameLine();
        if (ImGui::Button(emuenv.kernel.debugger.watch_memory ? lang.debug["unwatch_memory"].c_str() : lang.debug["watch_memory"].c_str())) {
            emuenv.kernel.debugger.watch_memory = !emuenv.kernel.debugger.watch_memory;
            emuenv.kernel.debugger.update_watches();
        }
        ImGui::SameLine();
        if (ImGui::Button(emuenv.kernel.debugger.watch_import_calls ? lang.debug["unwatch_import_calls"].c_str() : lang.debug["watch_import_calls"].c_str())) {
            emuenv.kernel.debugger.watch_import_calls = !emuenv.kernel.debugger.watch_import_calls;
            emuenv.kernel.debugger.update_watches();
        }

#ifdef TRACY_ENABLE
        // Tracy profiler settings
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Tracy Profiler");

        ImGui::Text("The Tracy profiler implementation in the emulator allows among other\n"
                    "things to track the functions that a game calls in real-time\n"
                    "and visualize them in a timeline with timings for every frame and audio buffer.");

        // Primitive Tracy implementation
        ImGui::Checkbox("Primitive implementation", &emuenv.cfg.tracy_primitive_impl);
        SetTooltipEx("The primitive Tracy implementation for HLE modules allows for\n"
                     "all HLE module calls to be logged without manual instrumentation needed.\n"
                     "However it is just a general workaround that doesn't count for statistic\n"
                     "analysis neither for trace searching on Tracy.\n\n"
                     "Due to the amount of functions being logged due to this implementation\n"
                     "Tracy logs can become gigabytes long in a matter of minutes. Because of this\n"
                     "it is only recommended to be used when the module(s) to debug aren't available for\n"
                     "advanced profiling or a more general overview of the function calls is needed and\n"
                     "in a PC with at least 12GB (Linux) or 16GB (Windows) of RAM.");

        // ImGui::Text("The Tracy profiler is not available in Release builds, please compile Vita3K\nfrom source using"
        // " either the RelWithDebInfo or Debug builds in order to use it.");

        // Text to display along the modules list
        const char *tracy_modules_list_label = "Available modules for advanced profiling\n\n"
                                               "Modules enabled for advanced profiling don't\n"
                                               "only provide function call timings but\n"
                                               "also log the arguments they were called\n"
                                               "with for every single function call\n"
                                               "except arguments driving a large amount\n"
                                               "of data such as large sized arrays.\n\n"
                                               "Advanced profiling requires functions to\n"
                                               "be manually instrumented in source code.";

        // Tracy modules list
        if (ImGui::BeginListBox(tracy_modules_list_label, { 0.0f, ImGui::GetTextLineHeightWithSpacing() * 8.25f + ImGui::GetStyle().FramePadding.y * 2.0f })) {
            // Get all HLE modules available for advanced profiling using Tracy. Do it only once.
            static std::vector<std::string> tracy_modules = tracy_module_utils::get_available_module_names();
            // For every HLE module available for advanced profiling using Tracy
            for (auto &module : tracy_modules) {
                bool activation_state = tracy_module_utils::is_tracy_active(module);
                // Create selectable item using module name and get activation state
                if (ImGui::Selectable(module.c_str(), activation_state)) {
                    // Change activation state if module is clicked/selected
                    if (activation_state) {
                        // Deactivate module
                        tracy_module_utils::set_tracy_active(module, false);
                        // Update config data by deleting the name of the module from the vector
                        std::erase(emuenv.cfg.tracy_advanced_profiling_modules, module);
                    } else {
                        // Activate module
                        tracy_module_utils::set_tracy_active(module, true);
                        // Update config data by appending the name of the module to the vector
                        emuenv.cfg.tracy_advanced_profiling_modules.push_back(module);
                    }
                }
            }
            ImGui::EndListBox();
        }
#endif // TRACY_ENABLE

        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    ImGui::EndTabBar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    static const auto BUTTON_SIZE = ImVec2(120.f * SCALE.x, 0.f);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - BUTTON_SIZE.x - (10.f * SCALE.x));
    if (ImGui::Button(common.common["close"].c_str(), BUTTON_SIZE))
        show_settings_dialog = false;
    ImGui::SameLine(0, 20.f * SCALE.x);
    const auto is_apply = !emuenv.io.app_path.empty() && (!is_custom_config || (emuenv.app_path == emuenv.io.app_path));
    const auto is_reboot = !emuenv.io.app_path.empty()
        && ((emuenv.renderer->current_backend != emuenv.backend_renderer)
#ifdef __ANDROID__
            || (config.custom_driver_name != emuenv.renderer->current_custom_driver)
#endif
            || (config.resolution_multiplier != emuenv.renderer->res_multiplier));
    if (ImGui::Button(is_apply ? (is_reboot ? lang.main_window["save_reboot"].c_str() : lang.main_window["save_apply"].c_str()) : lang.main_window["save_close"].c_str(), BUTTON_SIZE)) {
        save_config(gui, emuenv);
        if (is_apply)
            set_config(emuenv);
        show_settings_dialog = false;
    }
    SetTooltipEx(lang.main_window["keep_changes"].c_str());

    ImGui::ScrollWhenDragging();
    ImGui::End();
}

} // namespace gui
