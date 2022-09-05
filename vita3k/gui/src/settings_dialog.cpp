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
#include <display/state.h>
#include <host/dialog/filesystem.hpp>
#include <io/state.h>
#include <kernel/state.h>
#include <renderer/state.h>

#include <gui/functions.h>
#include <gui/state.h>

#include <emuenv/state.h>

#include <misc/cpp/imgui_stdlib.h>

#include <cpu/functions.h>

#include <string>
#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL.h>

#include <algorithm>
#include <pugixml.hpp>
#include <sstream>

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

static void get_modules_list(GuiState &gui, EmuEnvState &emuenv) {
    gui.modules.clear();

    const auto modules_path{ fs::path(emuenv.pref_path) / "vs0/sys/external/" };
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

static void reset_emulator(GuiState &gui, EmuEnvState &emuenv) {
    gui.configuration_menu.settings_dialog = false;
    gui.live_area.home_screen = false;

    // Clean and save new config value
    emuenv.cfg.auto_user_login = false;
    emuenv.cfg.user_id.clear();
    emuenv.cfg.pref_path = string_utils::wide_to_utf(emuenv.pref_path);
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
    std::filesystem::path emulator_path = "";
    host::dialog::filesystem::Result result = host::dialog::filesystem::pick_folder(emulator_path);

    if (result == host::dialog::filesystem::Result::SUCCESS && emulator_path.wstring() != emuenv.pref_path) {
        // Refresh the working paths
        emuenv.pref_path = emulator_path.wstring() + L'/';

        // TODO: Move app old to new path
        reset_emulator(gui, emuenv);
        LOG_INFO("Successfully moved Vita3K path to: {}", string_utils::wide_to_utf(emuenv.pref_path));
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
 * @param emuenv State of the emulated PlayStation Vita environment
 * @param app_path Path to the app or game to get the custom config for
 * @return true A custom config for the application has been found, and `config` has been set up with
 * the setting values contained in the custom config file.
 * @return false A custom config for the application has not been found or a custom config has been found
 * but it's corrupted or invalid.
 */
static bool get_custom_config(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto CUSTOM_CONFIG_PATH{ fs::path(emuenv.base_path) / "config" / fmt::format("config_{}.xml", app_path) };

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
                config.v_sync = gpu_child.attribute("v-sync").as_bool();
                config.anisotropic_filtering = gpu_child.attribute("anisotropic-filtering").as_int();
            }

            // Load System Config
            const auto system_child = config_child.child("system");
            if (!system_child.empty())
                config.pstv_mode = system_child.attribute("pstv-mode").as_bool();

            // Load Emulator Config
            if (!config_child.child("emulator").empty()) {
                const auto emulator_child = config_child.child("emulator");
                config.ngs_enable = emulator_child.attribute("enable-ngs").as_bool();
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

static int current_aniso_filter_log, max_aniso_filter_log;

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
    if (!get_custom_config(gui, emuenv, app_path)) {
        config.cpu_backend = emuenv.cfg.cpu_backend;
        config.cpu_opt = emuenv.cfg.cpu_opt;
        config.modules_mode = emuenv.cfg.modules_mode;
        config.lle_modules = emuenv.cfg.lle_modules;
        config.resolution_multiplier = emuenv.cfg.resolution_multiplier;
        config.disable_surface_sync = emuenv.cfg.disable_surface_sync;
        config.enable_fxaa = emuenv.cfg.enable_fxaa;
        config.v_sync = emuenv.cfg.v_sync;
        config.anisotropic_filtering = emuenv.cfg.anisotropic_filtering;
        config.pstv_mode = emuenv.cfg.pstv_mode;
        config.ngs_enable = emuenv.cfg.ngs_enable;
    }
    config_cpu_backend = set_cpu_backend(config.cpu_backend);
    current_aniso_filter_log = static_cast<int>(log2f(static_cast<float>(config.anisotropic_filtering)));
    max_aniso_filter_log = static_cast<int>(log2f(static_cast<float>(emuenv.renderer->get_max_anisotropic_filtering())));
    emuenv.app_path = app_path;
    get_modules_list(gui, emuenv);
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
        const auto CONFIG_PATH{ fs::path(emuenv.base_path) / "config" };
        const auto CUSTOM_CONFIG_PATH{ CONFIG_PATH / fmt::format("config_{}.xml", emuenv.app_path) };
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
        gpu_child.append_attribute("v-sync") = config.v_sync;
        gpu_child.append_attribute("anisotropic-filtering") = config.anisotropic_filtering;

        // System
        auto system_child = config_child.append_child("system");
        system_child.append_attribute("pstv-mode") = config.pstv_mode;

        // Emulator
        auto emulator_child = config_child.append_child("emulator");
        emulator_child.append_attribute("enable-ngs") = config.ngs_enable;

        const auto save_xml = custom_config_xml.save_file(CUSTOM_CONFIG_PATH.c_str());
        if (!save_xml)
            LOG_ERROR("Fail save custom config xml for app path: {}, in path: {}", emuenv.app_path, CONFIG_PATH.string());
    } else {
        emuenv.cfg.cpu_backend = config.cpu_backend;
        emuenv.cfg.cpu_opt = config.cpu_opt;
        emuenv.cfg.modules_mode = config.modules_mode;
        emuenv.cfg.lle_modules = config.lle_modules;
        emuenv.cfg.pstv_mode = config.pstv_mode;
        emuenv.cfg.resolution_multiplier = config.resolution_multiplier;
        emuenv.cfg.disable_surface_sync = config.disable_surface_sync;
        emuenv.cfg.enable_fxaa = config.enable_fxaa;
        emuenv.cfg.v_sync = config.v_sync;
        emuenv.cfg.anisotropic_filtering = config.anisotropic_filtering;
        emuenv.cfg.ngs_enable = config.ngs_enable;
    }
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
}

static void set_vsync_state(const bool &state) {
    if (state) {
        // Try adaptive vsync first, falling back to regular vsync.
        if (SDL_GL_SetSwapInterval(-1) < 0) {
            SDL_GL_SetSwapInterval(1);
        }
    } else
        SDL_GL_SetSwapInterval(0);

    LOG_INFO("V-Sync state: {}", state);
}

/**
 * @brief Set up the config parameters on the emulated PlayStation Vita environment
 * that are susceptible to vary via app-specific config files with the proper values
 * depending on whether app-specific config files are being used or not.
 *
 * @param gui State of the Vita3K GUI
 * @param emuenv State of the emulated PlayStation Vita environment
 * @param app_path Path to the app or game to get the custom config for
 */
void set_config(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    // If a config file is in use, call `get_custom_config()` and set the config
    // parameters with the values stored in the app-specific custom config file
    if (get_custom_config(gui, emuenv, app_path))
        emuenv.cfg.current_config = config;
    else {
        // Else inherit the values from the global emulator config
        emuenv.cfg.current_config.cpu_backend = emuenv.cfg.cpu_backend;
        emuenv.cfg.current_config.cpu_opt = emuenv.cfg.cpu_opt;
        emuenv.cfg.current_config.modules_mode = emuenv.cfg.modules_mode;
        emuenv.cfg.current_config.lle_modules = emuenv.cfg.lle_modules;
        emuenv.cfg.current_config.pstv_mode = emuenv.cfg.pstv_mode;
        emuenv.cfg.current_config.resolution_multiplier = emuenv.cfg.resolution_multiplier;
        emuenv.cfg.current_config.disable_surface_sync = emuenv.cfg.disable_surface_sync;
        emuenv.cfg.current_config.enable_fxaa = emuenv.cfg.enable_fxaa;
        emuenv.cfg.current_config.v_sync = emuenv.cfg.v_sync;
        emuenv.cfg.current_config.anisotropic_filtering = emuenv.cfg.anisotropic_filtering;
        emuenv.cfg.current_config.ngs_enable = emuenv.cfg.ngs_enable;
    }

    // can be changed while ingame
    emuenv.renderer->disable_surface_sync = emuenv.cfg.current_config.disable_surface_sync;
    emuenv.renderer->set_fxaa(emuenv.cfg.current_config.enable_fxaa);
    if (emuenv.backend_renderer == renderer::Backend::OpenGL)
        set_vsync_state(emuenv.cfg.current_config.v_sync);
    emuenv.renderer->set_anisotropic_filtering(emuenv.cfg.current_config.anisotropic_filtering);

    // No change it if app already running
    if (emuenv.io.title_id.empty()) {
        emuenv.kernel.cpu_backend = set_cpu_backend(emuenv.cfg.current_config.cpu_backend);
        emuenv.kernel.cpu_opt = emuenv.cfg.current_config.cpu_opt;
        emuenv.renderer->res_multiplier = emuenv.cfg.current_config.resolution_multiplier;
    }
}

void draw_settings_dialog(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.48f));
    const auto is_custom_config = gui.configuration_menu.custom_settings_dialog;
    auto &settings_dialog = is_custom_config ? gui.configuration_menu.custom_settings_dialog : gui.configuration_menu.settings_dialog;
    ImGui::Begin("##settings", &settings_dialog, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(gui.vita_font);
    ImGui::SetWindowFontScale(0.7f);
    auto settings_str = gui.lang.main_menubar.configuration["settings"].c_str();
    const auto title = is_custom_config ? fmt::format("{}: {} [{}]", settings_str, get_app_index(gui, emuenv.app_path)->title, emuenv.app_path) : settings_str;
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
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Modules Mode");
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
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Modules List");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your desired modules.");
            ImGui::Spacing();
            ImGui::PushItemWidth(240 * emuenv.dpi_scale);
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
            gui.module_search_bar.Draw("##module_search_bar", 200 * emuenv.dpi_scale);
            ImGui::Spacing();
            if (ImGui::Button("Clear List")) {
                config.lle_modules.clear();
                for (auto &m : gui.modules)
                    m.second = false;
            }
            ImGui::SameLine();
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "No modules present.\nPlease download and install the last PS Vita firmware.");
            if (ImGui::Button("Download Firmware"))
                open_path("https://www.playstation.com/en-us/support/hardware/psvita/system-software/");
        }
        if (ImGui::Button("Refresh List"))
            get_modules_list(gui, emuenv);
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
                ImGui::SetTooltip("Check the box to enable additional CPU JIT optimizations.");
        }
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // GPU
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("GPU")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();

        static const char *LIST_BACKEND_RENDERER[] = { "OpenGL", "Vulkan" };
        if (ImGui::Combo("Backend Renderer (Reboot to apply)", reinterpret_cast<int *>(&emuenv.backend_renderer), LIST_BACKEND_RENDERER, IM_ARRAYSIZE(LIST_BACKEND_RENDERER)))
            emuenv.cfg.backend_renderer = LIST_BACKEND_RENDERER[int(emuenv.backend_renderer)];
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your preferred backend renderer.");
        ImGui::Spacing();

        const bool is_vulkan = (emuenv.backend_renderer == renderer::Backend::Vulkan);
        if (is_vulkan) {
            const std::vector<std::string> gpu_list_str = emuenv.renderer->get_gpu_list();
            // must convert to a vector of char*
            std::vector<const char *> gpu_list;
            for (const auto &gpu : gpu_list_str)
                gpu_list.push_back(gpu.c_str());
            if (ImGui::Combo("GPU (Reboot to apply)", &emuenv.cfg.gpu_idx, gpu_list.data(), static_cast<int>(gpu_list.size())))
                ImGui::SetTooltip("Select the GPU Vita3K should run on.");
        }
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize("Internal Resolution Upscaling").x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Internal Resolution Upscaling");
        ImGui::Spacing();
        if (!emuenv.io.title_id.empty())
            ImGui::BeginDisabled();
        ImGui::PushItemWidth(-70.f * emuenv.dpi_scale);
        if (ImGui::SliderInt("##res_scal", &config.resolution_multiplier, 1, 8, fmt::format("{}x", config.resolution_multiplier).c_str(), ImGuiSliderFlags_None)) {
            if (config.resolution_multiplier > 1)
                config.disable_surface_sync = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enable upscaling for Vita3K.\nExperimental: games are not guaranteed to render properly at more than 1x");
        ImGui::SameLine();
        if ((config.resolution_multiplier == 1) && !config.disable_surface_sync)
            ImGui::BeginDisabled();
        ImGui::PushID("Res scal");
        if (ImGui::Button("Reset", ImVec2(60.f * emuenv.dpi_scale, 0))) {
            config.resolution_multiplier = 1;
            config.disable_surface_sync = false;
        }
        ImGui::PopID();
        if (!emuenv.io.title_id.empty() || ((config.resolution_multiplier == 1) && !config.disable_surface_sync))
            ImGui::EndDisabled();
        ImGui::Spacing();
        const auto res_scal = fmt::format("{}x{}", 960 * config.resolution_multiplier, 544 * config.resolution_multiplier);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(res_scal.c_str()).x / 2.f) - (35.f * emuenv.dpi_scale));
        ImGui::Text("%s", res_scal.c_str());
        if (!is_vulkan) {
            ImGui::Checkbox("Disable surface sync", &config.disable_surface_sync);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Speed hack, check the box to disable surface syncing between CPU and GPU.\nSurface syncing is needed by a few games.\nGive a big performance boost if disabled (in particular when upscaling is on).");
            ImGui::Spacing();
        }
        ImGui::Checkbox("Enable anti-aliasing (FXAA)", &config.enable_fxaa);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Anti-aliasing is a technique for smoothing out jagged edges.\n FXAA comes at almost no performance cost but makes games look slightly blurry.");
        if (!is_vulkan) {
            ImGui::SameLine();
            ImGui::Checkbox("V-Sync", &config.v_sync);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Disabling V-Sync can fix the speed issue in some games.\nIt is recommended to keep it enabled to avoid tearing.");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize("Anisotropic Filtering").x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Anisotropic Filtering");
        ImGui::Spacing();
        ImGui::PushItemWidth(-70.f * emuenv.dpi_scale);
        if (ImGui::SliderInt("##aniso_filter", &current_aniso_filter_log, 0, max_aniso_filter_log, fmt::format("{}x", config.anisotropic_filtering).c_str()))
            config.anisotropic_filtering = 1 << current_aniso_filter_log;
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Anisotropic filtering is a technique to enhance the image quality of surfaces which are slopped relative to the viewer.\nIt has no drawback but can impact performance.");
        ImGui::SameLine();
        if (config.anisotropic_filtering == 1)
            ImGui::BeginDisabled();
        ImGui::PushID("Aniso filter");
        if (ImGui::Button("Reset", ImVec2(60.f * emuenv.dpi_scale, 0))) {
            config.anisotropic_filtering = 1;
            current_aniso_filter_log = 0;
        }
        ImGui::PopID();
        if (config.anisotropic_filtering == 1)
            ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize("Shaders").x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Shaders");
        ImGui::Spacing();
        ImGui::Checkbox("Use shader cache", &emuenv.cfg.shader_cache);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable shader cache to pre-compile it at game startup\nUncheck to disable this feature.");
        if (emuenv.renderer->features.spirv_shader) {
            ImGui::SameLine();
            ImGui::Checkbox("Use Spir-V shader (deprecated)", &emuenv.cfg.spirv_shader);

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Pass generated Spir-V shader directly to driver.\nNote that some beneficial extensions will be disabled, "
                                  "and not all GPUs are compatible with this.");
            }
        }
        const auto shaders_cache_path{ fs::path(emuenv.base_path) / "cache/shaders" };
        if (fs::exists(shaders_cache_path) && !fs::is_empty(shaders_cache_path)) {
            ImGui::Spacing();
            if (ImGui::Button("Clean Shaders Cache and Log")) {
                fs::remove_all(shaders_cache_path);
                fs::remove_all(fs::path(emuenv.base_path) / "shaderlog");
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
        ImGui::RadioButton("Circle", &emuenv.cfg.sys_button, 0);
        ImGui::RadioButton("Cross", &emuenv.cfg.sys_button, 1);
        ImGui::Spacing();
        ImGui::Checkbox("PS TV Mode", &config.pstv_mode);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable PS TV Emulated mode.");
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Emulator
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("Emulator")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("Boot apps in full screen", &emuenv.cfg.boot_apps_full_screen);
        ImGui::Spacing();
        ImGui::Checkbox("Enable NGS support", &config.ngs_enable);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable support for advanced audio library NGS.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Combo("Log Level", &emuenv.cfg.log_level, "Trace\0Debug\0Info\0Warning\0Error\0Critical\0Off\0"))
            logging::set_level(static_cast<spdlog::level::level_enum>(emuenv.cfg.log_level));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your preferred log level.");
        ImGui::Spacing();
        ImGui::Checkbox("Archive Log", &emuenv.cfg.archive_log);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable Archiving Log.");
        ImGui::SameLine();
#ifdef USE_DISCORD
        ImGui::Checkbox("Discord Rich Presence", &emuenv.cfg.discord_rich_presence);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enables Discord Rich Presence to show what application you're running on Discord.");
#endif
        ImGui::Checkbox("Texture Cache", &emuenv.cfg.texture_cache);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable texture cache.");
        ImGui::Separator();
        const auto perfomance_overley_size = ImGui::CalcTextSize("Performance overlay").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (perfomance_overley_size / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Performance Overlay");
        ImGui::Spacing();
        ImGui::Checkbox("Performance Overlay", &emuenv.cfg.performance_overlay);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Display performance information on the screen as an overlay.");
        if (emuenv.cfg.performance_overlay) {
            ImGui::Combo("Detail", &emuenv.cfg.performance_overlay_detail, "Minimum\0Low\0Medium\0Maximum\0");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred perfomance overley detail.");
            ImGui::Combo("Position", &emuenv.cfg.performance_overlay_position, "Top Left\0Top Center\0Top Right\0Botttom Left\0Botttom Center\0Botttom Right\0");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred perfomance overley position.");
        }
        ImGui::Spacing();
#ifndef WIN32
        ImGui::Checkbox("Check to enable case-insensitive path finding on case sensitive filesystems. \nRESETS ON RESTART", &emuenv.io.case_isens_find_enabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Allows emulator to attempt searching for files regardless of case on non-Windows platforms");
#endif
        ImGui::Separator();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize("Emulated System Storage Folder").x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Emulated System Storage Folder");
        ImGui::Spacing();
        ImGui::PushItemWidth(320);
        ImGui::TextColored(GUI_COLOR_TEXT, "Current emulator folder: %s", emuenv.cfg.pref_path.c_str());
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (ImGui::Button("Change Emulator Path"))
            change_emulator_path(gui, emuenv);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Change Vita3K emulator folder path.\nYou will need to move your old folder to the new location manually.");
        if (emuenv.cfg.pref_path != emuenv.default_path) {
            ImGui::SameLine();
            if (ImGui::Button("Reset Emulator Path")) {
                if (string_utils::utf_to_wide(emuenv.default_path) != emuenv.pref_path) {
                    emuenv.pref_path = string_utils::utf_to_wide(emuenv.default_path);

                    // Refresh the working paths
                    reset_emulator(gui, emuenv);
                    LOG_INFO("Successfully restored default path for Vita3K files to: {}", string_utils::wide_to_utf(emuenv.pref_path));
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reset Vita3K emulator path to default.\nYou will need to move your old folder to the default location manually.");
        }
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // GUI
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("GUI")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("GUI visible", &emuenv.cfg.show_gui);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to show GUI after booting an application.");
        ImGui::SameLine();
        ImGui::Checkbox("Info bar visible", &emuenv.cfg.show_info_bar);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to show an info bar inside app selector.");
        ImGui::Spacing();
        ImGui::Checkbox("Live Area app screen", &emuenv.cfg.show_live_area_screen);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to open the Live Area by default when clicking on an application.\nIf disabled, right click on an application to open it.");
        ImGui::SameLine();
        ImGui::Checkbox("Grid Mode", &emuenv.cfg.apps_list_grid);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to set the app list to grid mode.");
        if (!emuenv.cfg.apps_list_grid) {
            ImGui::Spacing();
            ImGui::SliderInt("App Icon Size", &emuenv.cfg.icon_size, 64, 128);
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
            ImGui::Checkbox("Asia Region", &emuenv.cfg.asia_font_support);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Check this box to enable font support for Korean and Traditional Chinese.\nEnabling this will use more memory and will require you to restart the emulator.");
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "No firmware font package present.\nPlease download and install it.");
            if (ImGui::Button("Download Firmware Font Package"))
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
        ImGui::TextColored(GUI_COLOR_TEXT, "Current theme content id: %s", gui.users[emuenv.io.user_id].theme_id.c_str());
        if (gui.users[emuenv.io.user_id].theme_id != "default") {
            ImGui::Spacing();
            if (ImGui::Button("Reset Default Theme")) {
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
            if (ImGui::Checkbox("Using theme background", &gui.users[emuenv.io.user_id].use_theme_bg))
                save_user(gui, emuenv, emuenv.io.user_id);

        if (!gui.user_backgrounds.empty()) {
            ImGui::Spacing();
            if (ImGui::Button("Clean User Backgrounds")) {
                gui.user_backgrounds[gui.users[emuenv.io.user_id].backgrounds[gui.current_user_bg]] = {};
                gui.user_backgrounds.clear();
                if (!gui.theme_backgrounds.empty())
                    gui.users[emuenv.io.user_id].use_theme_bg = true;
                gui.users[emuenv.io.user_id].backgrounds.clear();
                save_user(gui, emuenv, emuenv.io.user_id);
            }
        }
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Current start background: %s", gui.users[emuenv.io.user_id].start_type.c_str());
        if (((gui.users[emuenv.io.user_id].theme_id == "default") && (gui.users[emuenv.io.user_id].start_type != "default")) || ((gui.users[emuenv.io.user_id].theme_id != "default") && (gui.users[emuenv.io.user_id].start_type != "theme"))) {
            ImGui::Spacing();
            if (ImGui::Button("Reset Start Background")) {
                gui.users[emuenv.io.user_id].start_path.clear();
                init_theme_start_background(gui, emuenv, gui.users[emuenv.io.user_id].theme_id);
                gui.users[emuenv.io.user_id].start_type = (gui.users[emuenv.io.user_id].theme_id == "default") ? "default" : "theme";
                save_user(gui, emuenv, emuenv.io.user_id);
            }
        }
        if (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty()) {
            ImGui::Spacing();
            ImGui::SliderFloat("Background Alpha", &emuenv.cfg.background_alpha, 0.999f, 0.000f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred transparent background effect.\nThe minimum slider is opaque and the maximum is transparent.");
        }
        if (!gui.theme_backgrounds.empty() || (gui.user_backgrounds.size() > 1)) {
            ImGui::Spacing();
            ImGui::SliderInt("Delay for backgrounds", &emuenv.cfg.delay_background, 4, 60);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select the delay in seconds before changing backgrounds.");
        }
        ImGui::Spacing();
        ImGui::SliderInt("Delay for start screen", &emuenv.cfg.delay_start, 10, 60);
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
        ImGui::Checkbox("Import logging", &emuenv.kernel.debugger.log_imports);
        ImGui::SameLine();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log module import symbols.");
        ImGui::Checkbox("Export logging", &emuenv.kernel.debugger.log_exports);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log module export symbols.");
        ImGui::Spacing();
        ImGui::Checkbox("Shader logging", &emuenv.cfg.log_active_shaders);
        ImGui::SameLine();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log shaders being used on each draw call.");
        ImGui::Checkbox("Uniform logging", &emuenv.cfg.log_uniforms);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log shader uniform names and values.");
        ImGui::SameLine();
        ImGui::Checkbox("Save color surfaces", &emuenv.cfg.color_surface_debug);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Save color surfaces to files.");
        ImGui::Spacing();
        ImGui::Checkbox("Texture dumping", &emuenv.cfg.dump_textures);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Dump textures to files");
        ImGui::SameLine();
        ImGui::Checkbox("ELF dumping", &emuenv.kernel.debugger.dump_elfs);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Dump loaded code as ELFs");
        ImGui::Spacing();
        if (ImGui::Button(emuenv.kernel.debugger.watch_code ? "Unwatch Code" : "Watch Code")) {
            emuenv.kernel.debugger.watch_code = !emuenv.kernel.debugger.watch_code;
            emuenv.kernel.debugger.update_watches();
        }
        ImGui::SameLine();
        if (ImGui::Button(emuenv.kernel.debugger.watch_memory ? "Unwatch Memory" : "Watch Memory")) {
            emuenv.kernel.debugger.watch_memory = !emuenv.kernel.debugger.watch_memory;
            emuenv.kernel.debugger.update_watches();
        }
        ImGui::Spacing();
        if (ImGui::Button(emuenv.kernel.debugger.watch_import_calls ? "Unwatch Import Calls" : "Watch Import Calls")) {
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
            for (auto &module : emuenv.cfg.tracy_available_advanced_profiling_modules) {
                // Get activation state and position in vector of activated modules
                bool activation_state = false;
                int index = -1;
                activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, module, &index);

                // Create selectable item using module name and get activation state
                if (ImGui::Selectable(module.c_str(), activation_state)) {
                    // Change activation state if module is clicked/selected
                    if (activation_state == true) {
                        // Deactivate module by deleting the name of the module from the vector
                        emuenv.cfg.tracy_advanced_profiling_modules.erase(emuenv.cfg.tracy_advanced_profiling_modules.begin() + index);
                    } else {
                        // Activate module by appending the name of the module to the vector
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
    static const auto BUTTON_SIZE = ImVec2(100.f * emuenv.dpi_scale, 0.f);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - BUTTON_SIZE.x - (10.f * emuenv.dpi_scale));
    if (ImGui::Button("Close", BUTTON_SIZE))
        settings_dialog = false;
    ImGui::SameLine(0, 20.f * emuenv.dpi_scale);
    const auto is_apply = !emuenv.io.app_path.empty() && (!is_custom_config || (emuenv.app_path == emuenv.io.app_path));
    if (ImGui::Button(is_apply ? "Save & Apply" : "Save", BUTTON_SIZE)) {
        save_config(gui, emuenv);
        if (is_apply)
            set_config(gui, emuenv, emuenv.io.app_path);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click on Save to keep your changes.");

    ImGui::End();
}

} // namespace gui
