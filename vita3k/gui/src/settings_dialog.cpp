// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include "imgui.h"
#include "private.h"

#include <config/functions.h>
#include <config/state.h>

#include <gui/functions.h>
#include <gui/state.h>

#include <host/state.h>

#include <misc/cpp/imgui_stdlib.h>

#include <cpu/functions.h>

#include <string>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <algorithm>
#include <nfd.h>
#include <pugixml.hpp>
#include <sstream>

namespace gui {

/**
 * @brief Struct used to abstract the code that manages app-specific config files
 *
 * This struct matches in type to the one found in `host.cfg.current_config` and is
 * used to collect the proper values that must be set in `host.cfg.current_config`
 * depending on whether an app-specific config file is being used or not.
 *
 * If an app-specific config file is loaded, then the values in this struct will be
 * set to those of the app-specific config file before getting used to set up `host.cfg.current_config`
 * with those same values. If an app-specific config isn't loaded, then the values in this struct
 * will be set those of the global emulator settings before getting used to set up `host.cfg.current_config`.
 */
static Config::CurrentConfig config;

static void get_modules_list(GuiState &gui, HostState &host) {
    gui.modules.clear();

    const auto modules_path{ fs::path(host.pref_path) / "vs0/sys/external/" };
    if (fs::exists(modules_path) && !fs::is_empty(modules_path)) {
        for (const auto &module : fs::directory_iterator(modules_path)) {
            if (module.path().extension() == ".suprx")
                gui.modules.push_back({ module.path().filename().replace_extension().string(), false });
        }

        for (auto &m : gui.modules)
            m.second = std::find(config.lle_modules.begin(), config.lle_modules.end(), m.first) != config.lle_modules.end();

        std::sort(gui.modules.begin(), gui.modules.end(), [](const auto &ma, const auto &mb) {
            if (ma.second == mb.second)
                return ma.first < mb.first;
            return ma.second;
        });
    }
}

static void reset_emulator(GuiState &gui, HostState &host) {
    gui.configuration_menu.settings_dialog = false;
    gui.live_area.app_selector = false;

    // Clean and save new config value
    host.cfg.auto_user_login = false;
    host.cfg.user_id.clear();
    host.cfg.pref_path = string_utils::wide_to_utf(host.pref_path);
    host.io.user_id.clear();
    config::serialize_config(host.cfg, host.cfg.config_path);

    // Clean User apps list
    gui.app_selector.user_apps.clear();

    get_modules_list(gui, host);
    get_sys_apps_title(gui, host);
    get_notice_list(host);
    get_users_list(gui, host);
    init_home(gui, host);
}

static void change_emulator_path(GuiState &gui, HostState &host) {
    nfdchar_t *emulator_path = nullptr;
    nfdresult_t result = NFD_PickFolder(nullptr, &emulator_path);

    if (result == NFD_OKAY && string_utils::utf_to_wide(emulator_path) != host.pref_path) {
        // Refresh the working paths
        host.pref_path = string_utils::utf_to_wide(emulator_path) + L'/';

        // TODO: Move app old to new path
        reset_emulator(gui, host);
        LOG_INFO("Successfully moved Vita3K path to: {}", string_utils::wide_to_utf(host.pref_path));
    }
}

static CPUBackend config_cpu_backend;

/**
 * @brief Set up `config` with the values contained in the custom config file of a certain PlayStation Vita application
 *
 * If a custom config is found, the configuration values found in the file will be assigned to
 * `config`.
 *
 * @param gui State of the Vita3K GUI
 * @param host State of the emulated PlayStation Vita environment
 * @param app_path Path to the app or game to get the custom config for
 * @return true A custom config for the application has been found, and `config` has been set up with
 * the setting values contained in the custom config file.
 * @return false A custom config for the application has not been found or a custom config has been found
 * but it's corrupted or invalid.
 */
static bool get_custom_config(GuiState &gui, HostState &host, const std::string &app_path) {
    const auto CUSTOM_CONFIG_PATH{ fs::path(host.base_path) / "config" / fmt::format("config_{}.xml", app_path) };

    if (fs::exists(CUSTOM_CONFIG_PATH)) {
        pugi::xml_document custom_config_xml;
        if (custom_config_xml.load_file(CUSTOM_CONFIG_PATH.c_str()) && !custom_config_xml.child("config").empty()) {
            config = {};
            // Config
            const auto config_child = custom_config_xml.child("config");

            // Load Core Config
            if (!config_child.child("core").empty()) {
                const auto core_child = config_child.child("core");
                config.lle_driver_user = core_child.attribute("lle-driver-user").as_bool();
                config.modules_mode = core_child.attribute("modules-mode").as_int();
                for (auto &m : core_child.child("lle-modules"))
                    config.lle_modules.push_back(m.text().as_string());
            }

            // Load CPU Config
            if (!config_child.child("cpu").empty()) {
                const auto cpu_child = config_child.child("cpu");
                config.cpu_backend = cpu_child.attribute("cpu-backend").as_string();
                config.cpu_opt = cpu_child.attribute("cpu-opt").as_bool();
            }

            // Load GPU Config
            if (!config_child.child("gpu").empty()) {
                const auto gpu_child = config_child.child("gpu");
                config.resolution_multiplier = gpu_child.attribute("resolution-multiplier").as_int();
                config.disable_surface_sync = gpu_child.attribute("disable-surface-sync").as_bool();
                config.enable_fxaa = gpu_child.attribute("enable-fxaa").as_bool();
            }

            // Load System Config
            const auto system_child = config_child.child("system");
            if (!system_child.empty())
                config.pstv_mode = system_child.attribute("pstv-mode").as_bool();

            // Load Emulator Config
            if (!config_child.child("emulator").empty()) {
                const auto emulator_child = config_child.child("emulator");
                config.disable_ngs = emulator_child.attribute("disable-ngs").as_bool();
            }

            return true;
        } else {
            LOG_ERROR("Custom config XML found is corrupted or invalid in path: {}", CUSTOM_CONFIG_PATH.string());
            fs::remove(CUSTOM_CONFIG_PATH);
            return false;
        }
    }

    return false;
}

static CPUBackend set_cpu_backend(std::string &cpu_backend) {
    return cpu_backend == "Dynarmic" ? CPUBackend::Dynarmic : CPUBackend::Unicorn;
}

/**
 * @brief Initialize the `config` struct with the values set in the global emulator config.
 *
 * @param gui State of the Vita3K GUI
 * @param host State of the emulated PlayStation Vita environment
 * @param app_path Path to the app or game to get the custom config for
 */
void init_config(GuiState &gui, HostState &host, const std::string &app_path) {
    // If no app-specific config file is being used for the initialized application,
    // set up `config` with the values set in the global emulator configuration
    if (!get_custom_config(gui, host, app_path)) {
        config.cpu_backend = host.cfg.cpu_backend;
        config.cpu_opt = host.cfg.cpu_opt;
        config.lle_driver_user = host.cfg.lle_driver_user;
        config.modules_mode = host.cfg.modules_mode;
        config.lle_modules = host.cfg.lle_modules;
        config.resolution_multiplier = host.cfg.resolution_multiplier;
        config.disable_surface_sync = host.cfg.disable_surface_sync;
        config.enable_fxaa = host.cfg.enable_fxaa;
        config.pstv_mode = host.cfg.pstv_mode;
        config.disable_ngs = host.cfg.disable_ngs;
    }
    config_cpu_backend = set_cpu_backend(config.cpu_backend);
    host.app_path = app_path;
    get_modules_list(gui, host);
    host.display.imgui_render = true;
    host.renderer->res_multiplier = config.resolution_multiplier;
    host.renderer->disable_surface_sync = config.disable_surface_sync;
    host.renderer->set_fxaa(config.enable_fxaa);
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
 * @param host State of the emulated PlayStation Vita environment
 */
static void save_config(GuiState &gui, HostState &host) {
    if (gui.configuration_menu.custom_settings_dialog) {
        const auto CONFIG_PATH{ fs::path(host.base_path) / "config" };
        const auto CUSTOM_CONFIG_PATH{ CONFIG_PATH / fmt::format("config_{}.xml", host.app_path) };
        if (!fs::exists(CONFIG_PATH))
            fs::create_directory(CONFIG_PATH);

        pugi::xml_document custom_config_xml;
        auto declarationUser = custom_config_xml.append_child(pugi::node_declaration);
        declarationUser.append_attribute("version") = "1.0";
        declarationUser.append_attribute("encoding") = "utf-8";

        // Config
        auto config_child = custom_config_xml.append_child("config");

        // Core
        auto core_child = config_child.append_child("core");
        core_child.append_attribute("lle-driver-user") = config.lle_driver_user;
        core_child.append_attribute("modules-mode") = config.modules_mode;
        auto enable_module = core_child.append_child("lle-modules");
        for (const auto &m : config.lle_modules)
            enable_module.append_child("module").append_child(pugi::node_pcdata).set_value(m.c_str());

        // CPU
        auto cpu_child = config_child.append_child("cpu");
        cpu_child.append_attribute("cpu-backend") = config.cpu_backend.c_str();
        cpu_child.append_attribute("cpu-opt") = config.cpu_opt;

        // GPU
        auto gpu_child = config_child.append_child("gpu");
        gpu_child.append_attribute("resolution-multiplier") = config.resolution_multiplier;
        gpu_child.append_attribute("disable-surface-sync") = config.disable_surface_sync;
        gpu_child.append_attribute("enable-fxaa") = config.enable_fxaa;

        // System
        auto system_child = config_child.append_child("system");
        system_child.append_attribute("pstv-mode") = config.pstv_mode;

        // Emulator
        auto emulator_child = config_child.append_child("emulator");
        emulator_child.append_attribute("disable-ngs") = config.disable_ngs;

        const auto save_xml = custom_config_xml.save_file(CUSTOM_CONFIG_PATH.c_str());
        if (!save_xml)
            LOG_ERROR("Fail save custom config xml for app path: {}, in path: {}", host.app_path, CONFIG_PATH.string());
    } else {
        host.cfg.cpu_backend = config.cpu_backend;
        host.cfg.cpu_opt = config.cpu_opt;
        host.cfg.lle_driver_user = config.lle_driver_user;
        host.cfg.modules_mode = config.modules_mode;
        host.cfg.lle_modules = config.lle_modules;
        host.cfg.pstv_mode = config.pstv_mode;
        host.cfg.resolution_multiplier = config.resolution_multiplier;
        host.cfg.disable_surface_sync = config.disable_surface_sync;
        host.cfg.enable_fxaa = config.enable_fxaa;
        host.cfg.disable_ngs = config.disable_ngs;
    }
    config::serialize_config(host.cfg, host.cfg.config_path);
}

/**
 * @brief Set up the config parameters on the emulated PlayStation Vita environment
 * that are susceptible to vary via app-specific config files with the proper values
 * depending on whether app-specific config files are being used or not.
 *
 * @param gui State of the Vita3K GUI
 * @param host State of the emulated PlayStation Vita environment
 * @param app_path Path to the app or game to get the custom config for
 */
void set_config(GuiState &gui, HostState &host, const std::string &app_path) {
    // If a config file is in use, call `get_custom_config()` and set the config
    // parameters with the values stored in the app-specific custom config file
    if (get_custom_config(gui, host, app_path)) {
        host.cfg.current_config.cpu_backend = config.cpu_backend;
        host.cfg.current_config.cpu_opt = config.cpu_opt;
        host.cfg.current_config.lle_driver_user = config.lle_driver_user;
        host.cfg.current_config.modules_mode = config.modules_mode;
        host.cfg.current_config.lle_modules = config.lle_modules;
        host.cfg.current_config.pstv_mode = config.pstv_mode;
        host.cfg.current_config.resolution_multiplier = config.resolution_multiplier;
        host.cfg.current_config.disable_surface_sync = config.disable_surface_sync;
        host.cfg.current_config.enable_fxaa = config.enable_fxaa;
        host.cfg.current_config.disable_ngs = config.disable_ngs;
    } else {
        // Else inherit the values from the global emulator config
        host.cfg.current_config.cpu_backend = host.cfg.cpu_backend;
        host.cfg.current_config.cpu_opt = host.cfg.cpu_opt;
        host.cfg.current_config.lle_driver_user = host.cfg.lle_driver_user;
        host.cfg.current_config.modules_mode = host.cfg.modules_mode;
        host.cfg.current_config.lle_modules = host.cfg.lle_modules;
        host.cfg.current_config.pstv_mode = host.cfg.pstv_mode;
        host.cfg.current_config.resolution_multiplier = host.cfg.resolution_multiplier;
        host.cfg.current_config.disable_surface_sync = host.cfg.disable_surface_sync;
        host.cfg.current_config.enable_fxaa = host.cfg.enable_fxaa;
        host.cfg.current_config.disable_ngs = host.cfg.disable_ngs;
    }
    // can be changed while ingame
    host.renderer->disable_surface_sync = host.cfg.current_config.disable_surface_sync;
    host.renderer->set_fxaa(host.cfg.enable_fxaa);
    // No change it if app already running
    if (host.io.title_id.empty()) {
        host.kernel.cpu_backend = set_cpu_backend(host.cfg.current_config.cpu_backend);
        host.kernel.cpu_opt = host.cfg.current_config.cpu_opt;
        host.renderer->res_multiplier = host.cfg.current_config.resolution_multiplier;
    }
}

void draw_settings_dialog(GuiState &gui, HostState &host) {
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.48f));
    const auto is_custom_config = gui.configuration_menu.custom_settings_dialog;
    auto &settings_dialog = is_custom_config ? gui.configuration_menu.custom_settings_dialog : gui.configuration_menu.settings_dialog;
    ImGui::Begin("##settings", &settings_dialog, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(gui.vita_font);
    ImGui::SetWindowFontScale(0.65f);
    const auto title = is_custom_config ? fmt::format("Settings: {} [{}]", get_app_index(gui, host.app_path)->title, host.app_path) : "Settings";
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (ImGui::CalcTextSize(title.c_str()).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title.c_str());
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::SetWindowFontScale(1.f);
    ImGui::BeginTabBar("SettingsTabBar", ImGuiTabBarFlags_None);

    // Core
    if (ImGui::BeginTabItem("Core")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (!gui.modules.empty()) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Modules mode");
            ImGui::Spacing();
            ImGui::Checkbox("LLE DriverUser", &config.lle_driver_user);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enable this to use driver_us modules (experimental).");
            ImGui::Spacing();
            for (auto m = 0; m < MODULES_MODE_COUNT; m++) {
                if (m)
                    ImGui::SameLine();
                ImGui::RadioButton(config_modules_mode[m][ModulesModeType::MODE], &config.modules_mode, m);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", config_modules_mode[m][ModulesModeType::DESCRIPTION]);
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Modules list");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your desired modules.");
            ImGui::Spacing();
            ImGui::PushItemWidth(240 * host.dpi_scale);
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
                }
                ImGui::EndListBox();
            }
            ImGui::PopItemWidth();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT, "Search modules");
            gui.module_search_bar.Draw("##module_search_bar", 200 * host.dpi_scale);
            ImGui::Spacing();
            if (ImGui::Button("Clear list")) {
                config.lle_modules.clear();
                for (auto &m : gui.modules)
                    m.second = false;
            }
            ImGui::SameLine();
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "No modules present.\nPlease download and install the PS Vita firmware.");
            if (ImGui::Button("Download Firmware"))
                open_path("https://www.playstation.com/en-us/support/hardware/psvita/system-software/");
        }
        if (ImGui::Button("Refresh list"))
            get_modules_list(gui, host);
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // CPU
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("CPU")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        static const char *LIST_CPU_BACKEND[] = { "Dynarmic", "Unicorn" };
        static const char *LIST_CPU_BACKEND_DISPLAY[] = { "Dynarmic", "Unicorn (deprecated)" };
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "CPU Backend");
        if (ImGui::Combo("##cpu_backend", reinterpret_cast<int *>(&config_cpu_backend), LIST_CPU_BACKEND_DISPLAY, IM_ARRAYSIZE(LIST_CPU_BACKEND_DISPLAY)))
            config.cpu_backend = LIST_CPU_BACKEND[int(config_cpu_backend)];
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your preferred CPU backend.");
        if (config_cpu_backend == CPUBackend::Dynarmic) {
            ImGui::Spacing();
            ImGui::Checkbox("Enable optimizations", &config.cpu_opt);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enables additional CPU JIT optimizations.");
        }
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // GPU
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("GPU")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
#ifdef USE_VULKAN
        static const char *LIST_BACKEND_RENDERER[] = { "OpenGL", "Vulkan" };
        if (ImGui::Combo("Backend renderer (Reboot to apply)", reinterpret_cast<int *>(&host.backend_renderer), LIST_BACKEND_RENDERER, IM_ARRAYSIZE(LIST_BACKEND_RENDERER)))
            host.cfg.backend_renderer = LIST_BACKEND_RENDERER[int(host.backend_renderer)];
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your preferred backend renderer.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
#endif
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize("Internal Resolution Upscaling").x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Internal Resolution Upscaling");
        ImGui::Spacing();
        if (!host.io.title_id.empty())
            ImGui::BeginDisabled();
        ImGui::PushItemWidth(-70.f * host.dpi_scale);
        if (ImGui::SliderInt("##res_scal", &config.resolution_multiplier, 1, 8, fmt::format("{}x", config.resolution_multiplier).c_str(), ImGuiSliderFlags_None)) {
            if (config.resolution_multiplier > 1)
                config.disable_surface_sync = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enable upscaling for Vita3K.\nExperimental: games are not guaranteed to render properly at more than 1x");
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(60.f * host.dpi_scale, 0))) {
            config.resolution_multiplier = 1;
            config.disable_surface_sync = false;
        }
        ImGui::Spacing();
        const auto res_scal = fmt::format("{}x{}", 960 * config.resolution_multiplier, 544 * config.resolution_multiplier);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(res_scal.c_str()).x / 2.f) - (35.f * host.dpi_scale));
        ImGui::Text("%s", res_scal.c_str());
        if (!host.io.title_id.empty())
            ImGui::EndDisabled();
        ImGui::Checkbox("Disable surface sync", &config.disable_surface_sync);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Speed hack, disables surface syncing between CPU and GPU.\nSurface syncing is needed by a few games.\nGives a big performance boost if disabled (in particular when upscaling is on).");
        ImGui::Spacing();
        ImGui::Checkbox("Enable anti-aliasing (FXAA)", &config.enable_fxaa);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Anti-aliasing is a technique for smoothing out jagged edges.\n FXAA comes at almost no performance cost but makes games look slightly blurry.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize("Shaders").x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Shaders");
        ImGui::Spacing();
        ImGui::Checkbox("Use shader cache", &host.cfg.shader_cache);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enables shader cache to pre-compile it at game startup\nUncheck to disable this feature.");
        ImGui::SameLine();
        if (host.renderer->features.spirv_shader) {
            ImGui::Checkbox("Use Spir-V shader (deprecated)", &host.cfg.spirv_shader);

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Pass generated Spir-V shader directly to driver.\nNote that some beneficial extensions will be disabled, "
                                  "and not all GPUs are compatible with this.");
            }
        }
        const auto shaders_cache_path{ fs::path(host.base_path) / "cache/shaders" };
        if (fs::exists(shaders_cache_path) && !fs::is_empty(shaders_cache_path)) {
            ImGui::Spacing();
            if (ImGui::Button("Clean Shaders Cache and Log")) {
                fs::remove_all(shaders_cache_path);
                fs::remove_all(fs::path(host.base_path) / "shaderlog");
            }
        }
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // System
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("System")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Enter button assignment \nSelect your 'Enter' button.");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("This is the button that is used as 'Confirm' in applications dialogs. \nSome applications don't use this and get default confirmation button.");
        ImGui::RadioButton("Circle", &host.cfg.sys_button, 0);
        ImGui::RadioButton("Cross", &host.cfg.sys_button, 1);
        ImGui::Spacing();
        ImGui::Checkbox("PS TV Mode", &config.pstv_mode);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check to enable PS TV mode.");
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Emulator
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("Emulator")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("Disable ngs support", &config.disable_ngs);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Disable support for advanced audio library ngs");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Combo("Log Level", &host.cfg.log_level, "Trace\0Debug\0Info\0Warning\0Error\0Critical\0Off\0"))
            logging::set_level(static_cast<spdlog::level::level_enum>(host.cfg.log_level));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your preferred log level.");
        ImGui::Spacing();
        ImGui::Checkbox("Archive Log", &host.cfg.archive_log);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enables log archiving.");
        ImGui::SameLine();
#ifdef USE_DISCORD
        ImGui::Checkbox("Discord Rich Presence", &host.cfg.discord_rich_presence);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enables Discord Rich Presence to show what application you're running on Discord.");
#endif
        ImGui::Checkbox("Texture Cache", &host.cfg.texture_cache);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable texture cache.");
        ImGui::Separator();
        const auto perfomance_overley_size = ImGui::CalcTextSize("Performance overlay").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (perfomance_overley_size / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Performance overlay");
        ImGui::Spacing();
        ImGui::Checkbox("Performance Overlay", &host.cfg.performance_overlay);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Display performance information on the screen as an overlay.");
        if (host.cfg.performance_overlay) {
            ImGui::Combo("Detail", &host.cfg.performance_overlay_detail, "Minimum\0Low\0Medium\0Maximum\0");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred perfomance overley detail.");
            ImGui::Combo("Position", &host.cfg.performance_overlay_position, "Top Left\0Top Center\0Top Right\0Botttom Left\0Botttom Center\0Botttom Right\0");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred perfomance overley position.");
        }
        ImGui::Spacing();
#ifndef WIN32
        ImGui::Checkbox("Check to enable case-insensitive path finding on case sensitive filesystems. \nRESETS ON RESTART", &host.io.case_isens_find_enabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Allows emulator to attempt searching for files regardless of case on non-Windows platforms");
#endif
        ImGui::Separator();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Emulated system storage folder");
        ImGui::Spacing();
        ImGui::PushItemWidth(320);
        ImGui::TextColored(GUI_COLOR_TEXT, "Current emulator folder: %s", host.cfg.pref_path.c_str());
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (ImGui::Button("Change emulator path"))
            change_emulator_path(gui, host);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Changes Vita3K emulator folder path.\nYou will need to move your old folder to the new location manually.");
        if (host.cfg.pref_path != host.default_path) {
            ImGui::SameLine();
            if (ImGui::Button("Reset emulator path")) {
                if (string_utils::utf_to_wide(host.default_path) != host.pref_path) {
                    host.pref_path = string_utils::utf_to_wide(host.default_path);

                    // Refresh the working paths
                    reset_emulator(gui, host);
                    LOG_INFO("Successfully restored default path for Vita3K files to: {}", string_utils::wide_to_utf(host.pref_path));
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Resets Vita3K emulator path to default.\nYou will need to move your old folder to the default location manually.");
        }
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // GUI
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("GUI")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("GUI visible", &host.cfg.show_gui);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to show GUI after booting an application.");
        ImGui::SameLine();
        ImGui::Checkbox("Info bar visible", &host.cfg.show_info_bar);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to show an info bar inside app selector.");
        ImGui::Spacing();
        ImGui::Checkbox("Live Area app screen", &host.cfg.show_live_area_screen);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to open the Live Area by default when clicking on an application.\nIf disabled, right click on an application to open it.");
        ImGui::SameLine();
        ImGui::Checkbox("Grid Mode", &host.cfg.apps_list_grid);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to set the app list to grid mode.");
        if (!host.cfg.apps_list_grid) {
            ImGui::Spacing();
            ImGui::SliderInt("App Icon Size", &host.cfg.icon_size, 64, 128);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred icon size.");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        const auto font_size = ImGui::CalcTextSize("Font support").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (font_size / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "Font support");
        ImGui::Spacing();
        if (gui.fw_font) {
            ImGui::Checkbox("Asia Region", &host.cfg.asia_font_support);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enables font support for Korean and Traditional Chinese.\nEnabling this will use more memory and will require you to restart the emulator.");
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "No firmware font package present.\nPlease download and install it.");
            if (ImGui::Button("Download firmware font package"))
                open_path("https://bit.ly/2P2rb0r");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Firmware font package is mandatory for some applications and also for Asian region font support in GUI.\nIt is also generally recommended for GUI");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        const auto title = ImGui::CalcTextSize("Theme & Background").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (title / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "Theme & Background");
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Current theme content id: %s", gui.users[host.io.user_id].theme_id.c_str());
        if (gui.users[host.io.user_id].theme_id != "default") {
            ImGui::Spacing();
            if (ImGui::Button("Reset Default Theme")) {
                gui.users[host.io.user_id].theme_id = "default";
                gui.users[host.io.user_id].use_theme_bg = false;
                if (init_theme(gui, host, "default"))
                    gui.users[host.io.user_id].use_theme_bg = true;
                gui.users[host.io.user_id].start_path.clear();
                gui.users[host.io.user_id].start_type = "default";
                save_user(gui, host, host.io.user_id);
                init_theme_start_background(gui, host, "default");
                init_apps_icon(gui, host, gui.app_selector.sys_apps);
            }
            ImGui::SameLine();
        }
        if (!gui.theme_backgrounds.empty())
            if (ImGui::Checkbox("Using theme background", &gui.users[host.io.user_id].use_theme_bg))
                save_user(gui, host, host.io.user_id);

        if (!gui.user_backgrounds.empty()) {
            ImGui::Spacing();
            if (ImGui::Button("Clean User Backgrounds")) {
                gui.user_backgrounds[gui.users[host.io.user_id].backgrounds[gui.current_user_bg]] = {};
                gui.user_backgrounds.clear();
                if (!gui.theme_backgrounds.empty())
                    gui.users[host.io.user_id].use_theme_bg = true;
                gui.users[host.io.user_id].backgrounds.clear();
                save_user(gui, host, host.io.user_id);
            }
        }
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Current start background: %s", gui.users[host.io.user_id].start_type.c_str());
        if (((gui.users[host.io.user_id].theme_id == "default") && (gui.users[host.io.user_id].start_type != "default")) || ((gui.users[host.io.user_id].theme_id != "default") && (gui.users[host.io.user_id].start_type != "theme"))) {
            ImGui::Spacing();
            if (ImGui::Button("Reset Start Background")) {
                gui.users[host.io.user_id].start_path.clear();
                init_theme_start_background(gui, host, gui.users[host.io.user_id].theme_id);
                gui.users[host.io.user_id].start_type = (gui.users[host.io.user_id].theme_id == "default") ? "default" : "theme";
                save_user(gui, host, host.io.user_id);
            }
        }
        if (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty()) {
            ImGui::Spacing();
            ImGui::SliderFloat("Background Alpha", &host.cfg.background_alpha, 0.999f, 0.000f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred transparent background effect.\nThe minimum slider is opaque and the maximum is transparent.");
        }
        if (!gui.theme_backgrounds.empty() || (gui.user_backgrounds.size() > 1)) {
            ImGui::Spacing();
            ImGui::SliderInt("Delay for backgrounds", &host.cfg.delay_background, 4, 60);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select the delay in seconds before changing backgrounds.");
        }
        ImGui::Spacing();
        ImGui::SliderInt("Delay for start screen", &host.cfg.delay_start, 10, 60);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select the delay in seconds before returning to the start screen.");
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Debug
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("Debug")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("Import logging", &host.kernel.debugger.log_imports);
        ImGui::SameLine();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log module import symbols.");
        ImGui::Checkbox("Export logging", &host.kernel.debugger.log_exports);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log module export symbols.");
        ImGui::Spacing();
        ImGui::Checkbox("Shader logging", &host.cfg.log_active_shaders);
        ImGui::SameLine();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log shaders being used on each draw call.");
        ImGui::Checkbox("Uniform logging", &host.cfg.log_uniforms);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log shader uniform names and values.");
        ImGui::SameLine();
        ImGui::Checkbox("Save color surfaces", &host.cfg.color_surface_debug);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Save color surfaces to files.");
        ImGui::Spacing();
        ImGui::Checkbox("Texture dumping", &host.cfg.dump_textures);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Dumps textures to files");
        ImGui::SameLine();
        ImGui::Checkbox("ELF Dumping", &host.kernel.debugger.dump_elfs);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Dump loaded code as ELFs");
        ImGui::Spacing();
        if (ImGui::Button(host.kernel.debugger.watch_code ? "Unwatch code" : "Watch code")) {
            host.kernel.debugger.watch_code = !host.kernel.debugger.watch_code;
            host.kernel.debugger.update_watches();
        }
        ImGui::SameLine();
        if (ImGui::Button(host.kernel.debugger.watch_memory ? "Unwatch memory" : "Watch memory")) {
            host.kernel.debugger.watch_memory = !host.kernel.debugger.watch_memory;
            host.kernel.debugger.update_watches();
        }
        ImGui::Spacing();
        if (ImGui::Button(host.kernel.debugger.watch_import_calls ? "Unwatch import calls" : "Watch import calls")) {
            host.kernel.debugger.watch_import_calls = !host.kernel.debugger.watch_import_calls;
            host.kernel.debugger.update_watches();
        }

#ifdef TRACY_ENABLE
        // Tracy profiler settings
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Tracy profiler");

        ImGui::Text("The Tracy profiler implementation in the emulator allows among other\n"
                    "things to track the functions that a game calls in real-time\n"
                    "and visualize them in a timeline with timings for every frame and audio buffer.");

        // Primitive Tracy implementation
        ImGui::Checkbox("Primitive implementation", &host.cfg.tracy_primitive_impl);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("The primitive Tracy implementation for HLE modules allows for\n"
                              "all HLE module calls to be logged without manual instrumentation needed.\n"
                              "However it is just a general workaround that doesn't count for statistic\n"
                              "analysis neither for trace searching on Tracy.\n\n"
                              "Due to the amount of functions being logged due to this implementation\n"
                              "Tracy logs can become gigabytes long in a matter of minutes. Because of this\n"
                              "it is only recommended to be used when the module(s) to debug aren't available for\n"
                              "advanced profiling or a more general overview of the function calls is needed and\n"
                              "in a PC with at least 12GB (Linux) or 16GB (Windows) of RAM.");
        }

        // ImGui::Text("The Tracy profiler is not available in Release builds, please compile Vita3K\nfrom source using"
        // " either the RelWithDebInfo or Debug builds in order to use it.");

        // Text to display along the modules list
        const std::string tracy_modules_list_label = "Available modules for advanced profiling\n\n"
                                                     "Modules enabled for advanced profiling don't\n"
                                                     "only provide function call timings but\n"
                                                     "also log the arguments they were called\n"
                                                     "with for every single function call\n"
                                                     "except arguments driving a large amount\n"
                                                     "of data such as large sized arrays.\n\n"
                                                     "Advanced profiling requires functions to\n"
                                                     "be manually instrumented in source code.";

        // Tracy modules list
        if (ImGui::BeginListBox(tracy_modules_list_label.c_str(), { 0.0f, ImGui::GetTextLineHeightWithSpacing() * 8.25f + ImGui::GetStyle().FramePadding.y * 2.0f })) {
            // For every HLE module available for advanced profiling using Tracy
            for (auto &module : host.cfg.tracy_available_advanced_profiling_modules) {
                // Get activation state and position in vector of activated modules
                bool activation_state = false;
                int index = -1;
                activation_state = config::is_tracy_advanced_profiling_active_for_module(host.cfg.tracy_advanced_profiling_modules, module, &index);

                // Create selectable item using module name and get activation state
                if (ImGui::Selectable(module.c_str(), activation_state)) {
                    // Change activation state if module is clicked/selected
                    if (activation_state == true) {
                        // Deactivate module by deleting the name of the module from the vector
                        host.cfg.tracy_advanced_profiling_modules.erase(host.cfg.tracy_advanced_profiling_modules.begin() + index);
                    } else {
                        // Activate module by appending the name of the module to the vector
                        host.cfg.tracy_advanced_profiling_modules.push_back(module);
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
    static const auto BUTTON_SIZE = ImVec2(100.f * host.dpi_scale, 0.f);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - BUTTON_SIZE.x - (10.f * host.dpi_scale));
    if (ImGui::Button("Close", BUTTON_SIZE))
        settings_dialog = false;
    ImGui::SameLine(0, 20.f * host.dpi_scale);
    const auto is_apply = !host.io.app_path.empty() && (!is_custom_config || (host.app_path == host.io.app_path));
    if (ImGui::Button(is_apply ? "Save & Apply" : "Save", BUTTON_SIZE)) {
        save_config(gui, host);
        if (is_apply)
            set_config(gui, host, host.io.app_path);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click on Save to keep your changes.");

    ImGui::End();
}

} // namespace gui
