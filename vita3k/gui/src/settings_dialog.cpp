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

#include <kernel/functions.h>

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

static bool change_pref_location(const std::string &input_path, const std::string &current_path) {
    if (fs::path(input_path).has_extension())
        return false;

    if (!fs::exists(input_path))
        fs::create_directories(input_path);

    try {
        fs::directory_iterator it{ current_path };
        while (it != fs::directory_iterator{}) {
            // TODO: Move Vita directories

            boost::system::error_code err;
            it.increment(err);
        }
    } catch (const std::exception &err) {
        return false;
    }
    return true;
}

static void reset_emulator(GuiState &gui, HostState &host) {
    gui.live_area.app_selector = false;
    get_modules_list(gui, host);
    refresh_app_list(gui, host);
    get_sys_apps_title(gui, host);
    get_notice_list(host);
    get_users_list(gui, host);
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

Config::CurrentConfig custom_config;

void get_modules_list(GuiState &gui, HostState &host) {
    gui.modules.clear();
    auto &lle_modules = gui.configuration_menu.custom_settings_dialog ? custom_config.lle_modules : host.cfg.lle_modules;

    const auto modules_path{ fs::path(host.pref_path) / "vs0/sys/external/" };
    if (fs::exists(modules_path) && !fs::is_empty(modules_path)) {
        for (const auto &module : fs::directory_iterator(modules_path)) {
            if (module.path().extension() == ".suprx")
                gui.modules.push_back({ module.path().filename().replace_extension().string(), false });
        }

        for (auto &m : gui.modules)
            m.second = std::find(lle_modules.begin(), lle_modules.end(), m.first) != lle_modules.end();

        std::sort(gui.modules.begin(), gui.modules.end(), [](const auto &ma, const auto &mb) {
            if (ma.second == mb.second)
                return ma.first < mb.first;
            return ma.second;
        });
    }
}

void init_custom_config(GuiState &gui, HostState &host, const std::string &app_path) {
    if (!get_custom_config(gui, host, app_path)) {
        custom_config.lle_kernel = host.cfg.lle_kernel;
        custom_config.auto_lle = host.cfg.auto_lle;
        custom_config.lle_modules = host.cfg.lle_modules;
        custom_config.disable_ngs = host.cfg.disable_ngs;
        custom_config.video_playing = host.cfg.video_playing;
    }
    get_modules_list(gui, host);
}

bool get_custom_config(GuiState &gui, HostState &host, const std::string &app_path) {
    custom_config = {};
    const auto CUSTOM_CONFIG_PATH{ fs::path(host.base_path) / "config" / fmt::format("config_{}.xml", app_path) };

    if (fs::exists(CUSTOM_CONFIG_PATH)) {
        pugi::xml_document custum_config_xml;
        if (custum_config_xml.load_file(CUSTOM_CONFIG_PATH.c_str())) {
            // Load Core Config
            if (!custum_config_xml.child("core").empty()) {
                const auto core_child = custum_config_xml.child("core");
                custom_config.lle_kernel = core_child.attribute("lle-kernel").as_bool();
                custom_config.auto_lle = core_child.attribute("auto-lle").as_bool();
                for (auto &m : core_child.child("lle-modules"))
                    custom_config.lle_modules.push_back(m.text().as_string());
            }
            // Load Emulator Config
            if (!custum_config_xml.child("emulator").empty()) {
                const auto emulator_child = custum_config_xml.child("emulator");
                custom_config.disable_ngs = emulator_child.attribute("disable-ngs").as_bool();
                custom_config.video_playing = emulator_child.attribute("video-playing").as_bool();
            }

            return true;
        } else {
            LOG_ERROR("Custrum config XML found is corrupted on path: {}", CUSTOM_CONFIG_PATH.string());
            fs::remove(CUSTOM_CONFIG_PATH);
            return false;
        }
    }

    return false;
}

static void save_custom_config(GuiState &gui, HostState &host) {
    const auto CONFIG_PATH{ fs::path(host.base_path) / "config" };
    const auto CUSTOM_CONFIG_PATH{ CONFIG_PATH / fmt::format("config_{}.xml", host.app_path) };
    if (!fs::exists(CONFIG_PATH))
        fs::create_directory(CONFIG_PATH);

    pugi::xml_document custum_config_xml;
    auto declarationUser = custum_config_xml.append_child(pugi::node_declaration);
    declarationUser.append_attribute("version") = "1.0";
    declarationUser.append_attribute("encoding") = "utf-8";

    // Core
    auto core_child = custum_config_xml.append_child("core");
    core_child.append_attribute("lle-kernel") = custom_config.lle_kernel;
    core_child.append_attribute("auto-lle") = custom_config.auto_lle;
    auto enable_module = core_child.append_child("lle-modules");
    for (const auto &m : custom_config.lle_modules)
        enable_module.append_child("module").append_child(pugi::node_pcdata).set_value(m.c_str());

    // Emulator
    auto emulator_child = custum_config_xml.append_child("emulator");
    emulator_child.append_attribute("disable-ngs") = custom_config.disable_ngs;
    emulator_child.append_attribute("video-playing") = custom_config.video_playing;

    const auto save_xml = custum_config_xml.save_file(CUSTOM_CONFIG_PATH.c_str());
    if (!save_xml)
        LOG_ERROR("Fail save custum config xml for app path: {}, in path: {}", host.app_path, CONFIG_PATH.string());
}

void set_config(GuiState &gui, HostState &host, const std::string &app_path) {
    if (get_custom_config(gui, host, app_path)) {
        host.cfg.current_config.lle_kernel = custom_config.lle_kernel;
        host.cfg.current_config.auto_lle = custom_config.auto_lle;
        host.cfg.current_config.lle_modules = custom_config.lle_modules;
        host.cfg.current_config.disable_ngs = custom_config.disable_ngs;
        host.cfg.current_config.video_playing = custom_config.video_playing;
    } else {
        host.cfg.current_config.lle_kernel = host.cfg.lle_kernel;
        host.cfg.current_config.auto_lle = host.cfg.auto_lle;
        host.cfg.current_config.lle_modules = host.cfg.lle_modules;
        host.cfg.current_config.disable_ngs = host.cfg.disable_ngs;
        host.cfg.current_config.video_playing = host.cfg.video_playing;
    }
}

void draw_settings_dialog(GuiState &gui, HostState &host) {
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    const auto is_custom_config = gui.configuration_menu.custom_settings_dialog;
    ImGui::Begin(is_custom_config ? ("Custom settings for " + host.app_path).c_str() : "Settings", is_custom_config ? &gui.configuration_menu.custom_settings_dialog : &gui.configuration_menu.settings_dialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::BeginTabBar("SettingsTabBar", ImGuiTabBarFlags_None);
    std::ostringstream link;

    // Core
    if (ImGui::BeginTabItem("Core")) {
        auto &lle_kernel = is_custom_config ? custom_config.lle_kernel : host.cfg.lle_kernel;
        auto &auto_lle = is_custom_config ? custom_config.auto_lle : host.cfg.auto_lle;
        auto &lle_modules = is_custom_config ? custom_config.lle_modules : host.cfg.lle_modules;
        ImGui::PopStyleColor();
        if (!gui.modules.empty()) {
            ImGui::Spacing();
            ImGui::Checkbox("Experimental: LLE libkernel & driver_us", &lle_kernel);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enable this for using libkernel and driver_us module (experimental).");
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Module Mode");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select Automatic or Manual mode for load modules list.");
            ImGui::Spacing();
            if (ImGui::RadioButton("Automatic", auto_lle))
                auto_lle = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("Manual", !auto_lle))
                auto_lle = false;
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
                    const auto module = std::find(lle_modules.begin(), lle_modules.end(), m.first);
                    const bool module_existed = (module != lle_modules.end());
                    if (!gui.module_search_bar.PassFilter(m.first.c_str()))
                        continue;
                    if (ImGui::Selectable(m.first.c_str(), &m.second, auto_lle ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
                        if (module_existed)
                            lle_modules.erase(module);
                        else
                            lle_modules.push_back(m.first);
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
                lle_modules.clear();
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

    if (!is_custom_config) {
        // GPU
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
        if (ImGui::BeginTabItem("GPU")) {
            ImGui::PopStyleColor();
#ifdef USE_VULKAN
            static const char *LIST_BACKEND_RENDERER[] = { "OpenGL", "Vulkan" };
            if (ImGui::Combo("Backend Renderer (Reboot for apply)", reinterpret_cast<int *>(&host.backend_renderer), LIST_BACKEND_RENDERER, IM_ARRAYSIZE(LIST_BACKEND_RENDERER)))
                host.cfg.backend_renderer = LIST_BACKEND_RENDERER[int(host.backend_renderer)];
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred backend renderer.");
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
    }

    // Emulator
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginTabItem("Emulator")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Checkbox("Disable experimental ngs support", is_custom_config ? &custom_config.disable_ngs : &host.cfg.disable_ngs);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Disable experimental support for advanced audio library ngs");
        ImGui::Checkbox("Enable video playing support", is_custom_config ? &custom_config.video_playing : &host.cfg.video_playing);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable video player.\nOn some game, disable it is required for more progress.");
        ImGui::Spacing();
        if (!is_custom_config) {
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
        }
        ImGui::EndTabItem();
    } else
        ImGui::PopStyleColor();

    if (!is_custom_config) {
        // GUI
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
        if (ImGui::BeginTabItem("GUI")) {
            ImGui::PopStyleColor();
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
            ImGui::Checkbox("Log Imports", &host.cfg.log_imports);
            ImGui::SameLine();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Log module import symbols.");
            ImGui::Checkbox("Log Exports", &host.cfg.log_exports);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Log module export symbols.");
            ImGui::Spacing();
            ImGui::Checkbox("Log Shaders", &host.cfg.log_active_shaders);
            ImGui::SameLine();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Log shaders being used on each draw call.");
            ImGui::Checkbox("Enable Stack Traceback", &host.cfg.stack_traceback);
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
            ImGui::Spacing();
            if (ImGui::Button(host.kernel.watch_code ? "Unwatch code" : "Watch code")) {
                host.kernel.watch_code = !host.kernel.watch_code;
                update_watches(host.kernel);
            }
            ImGui::SameLine();
            if (ImGui::Button(host.kernel.watch_memory ? "Unwatch memory" : "Watch memory")) {
                host.kernel.watch_memory = !host.kernel.watch_memory;
                update_watches(host.kernel);
            }
            ImGui::Spacing();
            if (ImGui::Button(host.kernel.watch_import_calls ? "Unwatch import calls" : "Watch import calls")) {
                host.kernel.watch_import_calls = !host.kernel.watch_import_calls;
                update_watches(host.kernel);
            }
            ImGui::EndTabItem();
        } else
            ImGui::PopStyleColor();
    }

    ImGui::EndTabBar();

    if (host.cfg.overwrite_config) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        static const auto BUTTON_SIZE = ImVec2(60.f * host.dpi_scale, 0.f);
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - BUTTON_SIZE.x - (10.f * host.dpi_scale));
        if (ImGui::Button("Apply", BUTTON_SIZE))
            set_config(gui, host, host.app_path);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click on Apply if you want change setting with app current running.");
        ImGui::SameLine(0, 20.f * host.dpi_scale);
        if (ImGui::Button("Save", BUTTON_SIZE)) {
            if (!is_custom_config)
                config::serialize_config(host.cfg, host.cfg.config_path);
            else
                save_custom_config(gui, host);
        }
        if (!is_custom_config && ImGui::IsItemHovered())
            ImGui::SetTooltip("Click on Save is required to keep changes after reboot.");
    }

    ImGui::End();
}

} // namespace gui
