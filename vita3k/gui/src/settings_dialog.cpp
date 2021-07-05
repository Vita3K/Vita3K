// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <config/functions.h>
#include <config/state.h>

#include <gui/functions.h>
#include <gui/state.h>

#include <host/state.h>

#include <misc/cpp/imgui_stdlib.h>

#include <cpu/functions.h>

#include <util/fs.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <algorithm>
#include <nfd.h>
#include <pugixml.hpp>
#include <sstream>

namespace gui {

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

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
    gui.live_area.app_selector = false;
    get_modules_list(gui, host);
    refresh_app_list(gui, host);
    get_sys_apps_title(gui, host);
    get_notice_list(host);
    get_users_list(gui, host);
    save_apps_cache(gui, host);
    init_home(gui, host);
    gui.configuration_menu.settings_dialog = false;
}

static void change_emulator_path(GuiState &gui, HostState &host) {
    nfdchar_t *emulator_path = nullptr;
    nfdresult_t result = NFD_PickFolder(nullptr, &emulator_path);

    if (result == NFD_OKAY && string_utils::utf_to_wide(emulator_path) != host.pref_path) {
        // Refresh the working paths
        host.cfg.pref_path = string_utils::wide_to_utf(string_utils::utf_to_wide(emulator_path) + L'/');
        host.pref_path = string_utils::utf_to_wide(host.cfg.pref_path);

        config::serialize_config(host.cfg, host.cfg.config_path);

        // TODO: Move app old to new path
        reset_emulator(gui, host);
        LOG_INFO("Successfully moved Vita3K path to: {}", string_utils::wide_to_utf(host.pref_path));
    }
}

static CPUBackend config_cpu_backend;

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
                config.lle_kernel = core_child.attribute("lle-kernel").as_bool();
                config.auto_lle = core_child.attribute("auto-lle").as_bool();
                for (auto &m : core_child.child("lle-modules"))
                    config.lle_modules.push_back(m.text().as_string());
            }

            // Load CPU Config
            if (!config_child.child("cpu").empty()) {
                const auto cpu_child = config_child.child("cpu");
                config.cpu_backend = cpu_child.attribute("cpu-backend").as_string();
                config.cpu_opt = cpu_child.attribute("cpu-opt").as_bool();
            }

            // Load Emulator Config
            if (!config_child.child("emulator").empty()) {
                const auto emulator_child = config_child.child("emulator");
                config.disable_at9_decoder = emulator_child.attribute("disable-at9-decoder").as_bool();
                config.disable_ngs = emulator_child.attribute("disable-ngs").as_bool();
                config.video_playing = emulator_child.attribute("video-playing").as_bool();
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
    return cpu_backend == "Unicorn" ? CPUBackend::Unicorn : CPUBackend::Dynarmic;
}

void init_config(GuiState &gui, HostState &host, const std::string &app_path) {
    if (!get_custom_config(gui, host, app_path)) {
        config.cpu_backend = host.cfg.cpu_backend;
        config.cpu_opt = host.cfg.cpu_opt;
        config.lle_kernel = host.cfg.lle_kernel;
        config.auto_lle = host.cfg.auto_lle;
        config.lle_modules = host.cfg.lle_modules;
        config.disable_at9_decoder = host.cfg.disable_at9_decoder;
        config.disable_ngs = host.cfg.disable_ngs;
        config.video_playing = host.cfg.video_playing;
    }
    config_cpu_backend = set_cpu_backend(config.cpu_backend);
    host.app_path = app_path;
    get_modules_list(gui, host);
    host.display.imgui_render = true;
}

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
        core_child.append_attribute("lle-kernel") = config.lle_kernel;
        core_child.append_attribute("auto-lle") = config.auto_lle;
        auto enable_module = core_child.append_child("lle-modules");
        for (const auto &m : config.lle_modules)
            enable_module.append_child("module").append_child(pugi::node_pcdata).set_value(m.c_str());

        // CPU
        auto cpu_child = config_child.append_child("cpu");
        cpu_child.append_attribute("cpu-backend") = config.cpu_backend.c_str();
        cpu_child.append_attribute("cpu-opt") = config.cpu_opt;

        // Emulator
        auto emulator_child = config_child.append_child("emulator");
        emulator_child.append_attribute("disable-at9-decoder") = config.disable_at9_decoder;
        emulator_child.append_attribute("disable-ngs") = config.disable_ngs;
        emulator_child.append_attribute("video-playing") = config.video_playing;

        const auto save_xml = custom_config_xml.save_file(CUSTOM_CONFIG_PATH.c_str());
        if (!save_xml)
            LOG_ERROR("Fail save custom config xml for app path: {}, in path: {}", host.app_path, CONFIG_PATH.string());
    } else {
        host.cfg.cpu_backend = config.cpu_backend;
        host.cfg.cpu_opt = config.cpu_opt;
        host.cfg.lle_kernel = config.lle_kernel;
        host.cfg.auto_lle = config.auto_lle;
        host.cfg.lle_modules = config.lle_modules;
        host.cfg.disable_at9_decoder = config.disable_at9_decoder;
        host.cfg.disable_ngs = config.disable_ngs;
        host.cfg.video_playing = config.video_playing;
    }
    config::serialize_config(host.cfg, host.cfg.config_path);
}

void set_config(GuiState &gui, HostState &host, const std::string &app_path) {
    if (get_custom_config(gui, host, app_path)) {
        host.cfg.current_config.cpu_backend = config.cpu_backend;
        host.cfg.current_config.cpu_opt = config.cpu_opt;
        host.cfg.current_config.lle_kernel = config.lle_kernel;
        host.cfg.current_config.auto_lle = config.auto_lle;
        host.cfg.current_config.lle_modules = config.lle_modules;
        host.cfg.current_config.disable_at9_decoder = config.disable_at9_decoder;
        host.cfg.current_config.disable_ngs = config.disable_ngs;
        host.cfg.current_config.video_playing = config.video_playing;
    } else {
        host.cfg.current_config.cpu_backend = host.cfg.cpu_backend;
        host.cfg.current_config.cpu_opt = host.cfg.cpu_opt;
        host.cfg.current_config.lle_kernel = host.cfg.lle_kernel;
        host.cfg.current_config.auto_lle = host.cfg.auto_lle;
        host.cfg.current_config.lle_modules = host.cfg.lle_modules;
        host.cfg.current_config.disable_at9_decoder = host.cfg.disable_at9_decoder;
        host.cfg.current_config.disable_ngs = host.cfg.disable_ngs;
        host.cfg.current_config.video_playing = host.cfg.video_playing;
    }
    // No change it if app already running
    if (host.io.title_id.empty()) {
        host.kernel.cpu_backend = set_cpu_backend(host.cfg.current_config.cpu_backend);
        host.kernel.cpu_opt = host.cfg.current_config.cpu_opt;
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
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, title.c_str());
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::SetWindowFontScale(1.f);
    ImGui::BeginTabBar("SettingsTabBar", ImGuiTabBarFlags_None);
    std::ostringstream link;

    // Core
    if (ImGui::BeginTabItem("Core")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (!gui.modules.empty()) {
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Modules Mode");
            ImGui::Spacing();
            ImGui::Checkbox("Experimental: LLE libkernel & driver_us", &config.lle_kernel);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enable this for using libkernel and driver_us module (experimental).");
            ImGui::Spacing();
            if (ImGui::RadioButton("Automatic", config.auto_lle))
                config.auto_lle = true;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select Automatic mode for using modules list pre-set.");
            ImGui::SameLine();
            if (ImGui::RadioButton("Manual", !config.auto_lle))
                config.auto_lle = false;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select Manual mode for load custom modules list.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Modules List");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your desired modules.");
            ImGui::Spacing();
            ImGui::PushItemWidth(240 * host.dpi_scale);
            if (ImGui::ListBoxHeader("##modules_list", static_cast<int>(gui.modules.size()), 8)) {
                for (auto &m : gui.modules) {
                    const auto module = std::find(config.lle_modules.begin(), config.lle_modules.end(), m.first);
                    const bool module_existed = (module != config.lle_modules.end());
                    if (!gui.module_search_bar.PassFilter(m.first.c_str()))
                        continue;
                    if (ImGui::Selectable(m.first.c_str(), &m.second, config.auto_lle ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
                        if (module_existed)
                            config.lle_modules.erase(module);
                        else
                            config.lle_modules.push_back(m.first);
                    }
                }
                ImGui::ListBoxFooter();
            }
            ImGui::PopItemWidth();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT, "Modules Search");
            gui.module_search_bar.Draw("##module_search_bar", 200 * host.dpi_scale);
            ImGui::Spacing();
            if (ImGui::Button("Clear list")) {
                config.lle_modules.clear();
                for (auto &m : gui.modules)
                    m.second = false;
            }
            ImGui::SameLine();
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "No modules present.\nPlease download and install the last firmware.");
            if (ImGui::Button("Download Firmware")) {
                std::string firmware_url = "https://www.playstation.com/en-us/support/hardware/psvita/system-software/";
                link << OS_PREFIX << firmware_url;
                system(link.str().c_str());
            }
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
        static const char *LIST_CPU_BACKEND[] = { "Unicorn", "Dynarmic" };
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Cpu Backend");
        if (ImGui::Combo("##cpu_backend", reinterpret_cast<int *>(&config_cpu_backend), LIST_CPU_BACKEND, IM_ARRAYSIZE(LIST_CPU_BACKEND)))
            config.cpu_backend = LIST_CPU_BACKEND[int(config_cpu_backend)];
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your preferred cpu backend.");
        if (config_cpu_backend == CPUBackend::Dynarmic) {
            ImGui::Spacing();
            ImGui::Checkbox("Enable optimizations", &config.cpu_opt);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enable additional CPU JIT optimizations.");
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
        if (ImGui::Combo("Backend Renderer (Reboot for apply)", reinterpret_cast<int *>(&host.backend_renderer), LIST_BACKEND_RENDERER, IM_ARRAYSIZE(LIST_BACKEND_RENDERER)))
            host.cfg.backend_renderer = LIST_BACKEND_RENDERER[int(host.backend_renderer)];
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your preferred backend renderer.");
        ImGui::Spacing();
#endif
        if (host.renderer->features.spirv_shader) {
            ImGui::Checkbox("Use Spir-V shader", &host.cfg.spirv_shader);

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Pass generated Spir-V shader directly to driver.\nNote that some beneficial extensions will be disabled, "
                                  "and not all GPU are compatible with this.");
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
        ImGui::TextColored(GUI_COLOR_TEXT, "Enter Button Assignment \nSelect your 'Enter' Button.");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("This is the button that is used as 'Confirm' in applications dialogs. \nSome applications don't use this and get default confirmation button.");
        ImGui::RadioButton("Circle", &host.cfg.sys_button, 0);
        ImGui::RadioButton("Cross", &host.cfg.sys_button, 1);
        ImGui::Spacing();
        ImGui::Checkbox("Emulated Console \nSelect your Console mode.", &host.cfg.pstv_mode);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable PS TV mode.");
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Emulator
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("Emulator")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("Disable At9 audio decoder", &config.disable_at9_decoder);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enable this options that disables AT9 audio decoder.\nIt is required to prevent the crash in certain games such as Gravity (Rush/Daze) for the time being.");
        ImGui::Checkbox("Disable experimental ngs support", &config.disable_ngs);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Disable experimental support for advanced audio library ngs");
        ImGui::Checkbox("Enable video playing support", &config.video_playing);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable video player.\nOn some game, disable it is required for more progress.");
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
            ImGui::SetTooltip("Check the box to enable Archiving Log.");
        ImGui::SameLine();
#ifdef USE_DISCORD
        ImGui::Checkbox("Discord Rich Presence", &host.cfg.discord_rich_presence);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enables Discord Rich Presence to show what application you're running on discord");
#endif
        ImGui::Checkbox("Performance overlay", &host.cfg.performance_overlay);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Display performance information on the screen as an overlay.");
        ImGui::SameLine();
        ImGui::Checkbox("Texture Cache", &host.cfg.texture_cache);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable texture cache.");
#ifndef WIN32
        ImGui::Checkbox("Check to enable case-insensitive path finding on case sensitive filesystems. \nRESETS ON RESTART", &host.io.case_isens_find_enabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Allows emulator to attempt searching for files regardless of case on non-windows platforms");
#endif
        ImGui::Separator();
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "Emulated System Storage Folder");
        ImGui::Spacing();
        ImGui::PushItemWidth(320);
        ImGui::TextColored(GUI_COLOR_TEXT, "Current emulator folder: %s", host.cfg.pref_path.c_str());
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (ImGui::Button("Change Emulator Path"))
            change_emulator_path(gui, host);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Change Vita3K emulator path like wanted.\nNeed move folder old to new manually.");
        if (host.cfg.pref_path != host.default_path) {
            ImGui::SameLine();
            if (ImGui::Button("Reset Path Emulator")) {
                if (string_utils::utf_to_wide(host.default_path) != host.pref_path) {
                    host.pref_path = string_utils::utf_to_wide(host.default_path);
                    host.cfg.pref_path = string_utils::wide_to_utf(host.pref_path);

                    config::serialize_config(host.cfg, host.cfg.config_path);

                    // Refresh the working paths
                    reset_emulator(gui, host);
                    LOG_INFO("Successfully restore default path for Vita3K files to: {}", string_utils::wide_to_utf(host.pref_path));
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reset Vita3K emulator path to default.\nNeed move folder old to default manually.");
        }
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // GUI
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("GUI")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("GUI Visible", &host.cfg.show_gui);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to show GUI after booting a application.");
        ImGui::SameLine();
        ImGui::Checkbox("Live Area App Screen", &host.cfg.show_live_area_screen);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to open Live Area by default when clicking on a application.\nIf disabled, use the right click on application to open it.");
        ImGui::Spacing();
        ImGui::Checkbox("Grid Mode", &host.cfg.apps_list_grid);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable app list in grid mode.");
        if (!host.cfg.apps_list_grid) {
            ImGui::Spacing();
            ImGui::SliderInt("App Icon Size", &host.cfg.icon_size, 64, 128);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred icon size.");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        const auto font_size = ImGui::CalcTextSize("Font Support").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (font_size / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "Font Support");
        ImGui::Spacing();
        if (gui.fw_font) {
            ImGui::Checkbox("Asia Region", &host.cfg.asia_font_support);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Check this box to enable font support for Korean and Traditional Chinese.\nEnabling this will use more memory and will require you to restart the emulator.");
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "No firmware font package present.\nPlease download and install it.");
            if (ImGui::Button("Download firmware font package")) {
                link << OS_PREFIX << "https://bit.ly/2P2rb0r";
                system(link.str().c_str());
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Firmware font package is mandatory for some applications and also for asian region font support in gui.\nIt is also generally recommended for gui");
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
            ImGui::SliderInt("Delay for backgrounds", &host.cfg.delay_background, 4, 32);
        }
        ImGui::Spacing();
        ImGui::SliderInt("Delay for start screen", &host.cfg.delay_start, 10, 60);
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    // Debug
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("Debug")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("Log Imports", &host.kernel.debugger.log_imports);
        ImGui::SameLine();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log module import symbols.");
        ImGui::Checkbox("Log Exports", &host.kernel.debugger.log_exports);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log module export symbols.");
        ImGui::Spacing();
        ImGui::Checkbox("Log Shaders", &host.cfg.log_active_shaders);
        ImGui::SameLine();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log shaders being used on each draw call.");
        ImGui::Checkbox("Log Uniforms", &host.cfg.log_uniforms);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log shader uniform names and values.");
        ImGui::SameLine();
        ImGui::Checkbox("Save color surfaces", &host.cfg.color_surface_debug);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Save color surfaces to files.");
        ImGui::Spacing();
        ImGui::Checkbox("Dump textures", &host.cfg.dump_textures);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Dump textures to files");
        ImGui::SameLine();
        ImGui::Checkbox("Dump elfs", &host.kernel.debugger.dump_elfs);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Dump loaded code as elfs");
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
        ImGui::SetTooltip("Click on Save is required to keep changes.");

    ImGui::End();
}

} // namespace gui
