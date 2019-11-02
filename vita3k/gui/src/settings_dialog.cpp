// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

#include <config/config.h>
#include <config/functions.h>

#include <gui/functions.h>
#include <gui/state.h>

#include <host/state.h>

#include <misc/cpp/imgui_stdlib.h>

#include <util/fs.h>
#include <util/log.h>

#include <algorithm>
#include <nfd.h>

namespace gui {

static const char *LIST_SYS_LANG[] = {
    "Japanese", "American English", "French", "Spanish", "German", "Italian", "Dutch", "Portugal Portuguese",
    "Russian", "Korean", "Traditional Chinese", "Simplified Chinese", "Finnish", "Swedish",
    "Danish", "Norwegian", "Polish", "Brazil Portuguese", "British English", "Turkish"
};

constexpr int SYS_LANG_COUNT = IM_ARRAYSIZE(LIST_SYS_LANG);

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

static void change_emulator_path(GuiState &gui, HostState &host) {
    nfdchar_t *emulator_path = nullptr;
    nfdresult_t result = NFD_PickFolder(nullptr, &emulator_path);

    if (result == NFD_OKAY && emulator_path != host.pref_path) {
        // Refresh the working paths
        host.cfg.pref_path = static_cast<std::string>(emulator_path) + '/';
        host.pref_path = host.cfg.pref_path;

        config::serialize_config(host.cfg, host.cfg.config_path);

        // TODO: Move game old to new path
        get_modules_list(gui, host);
        refresh_game_list(gui, host);
        LOG_INFO("Successfully moved Vita3K path to: {}", host.pref_path);
    }
}

static bool change_user_image_background(GuiState &gui, HostState &host) {
    nfdchar_t *image_path = nullptr;
    nfdresult_t result = NFD_OpenDialog("bmp,gif,jpg,png,tif", nullptr, &image_path);

    if (result == NFD_OKAY && host.cfg.background_image != static_cast<std::string>(image_path)) {
        const std::string image_path_str = static_cast<std::string>(image_path);

        if (gui.user_backgrounds.find(image_path_str) == gui.user_backgrounds.end())
            init_background(gui, image_path_str);
        else
            gui.current_background = gui.user_backgrounds[image_path_str];

        if (gui.user_backgrounds.find(image_path_str) != gui.user_backgrounds.end()) {
            host.cfg.background_image = image_path_str;
            return true;
        } else
            return false;

    } else
        return false;
}

void get_modules_list(GuiState &gui, HostState &host) {
    gui.modules.clear();

    const auto modules_path{ fs::path(host.pref_path) / "vs0/sys/external/" };
    if (fs::exists(modules_path) && !fs::is_empty(modules_path)) {
        fs::recursive_directory_iterator end;
        const std::string ext = ".suprx";
        for (fs::recursive_directory_iterator m(modules_path); m != end; ++m) {
            const fs::path lle = *m;
            if (m->path().extension() == ext)
                gui.modules.push_back({ lle.filename().replace_extension().string(), false });
        }

        for (auto &m : gui.modules)
            m.second = std::find(host.cfg.lle_modules.begin(), host.cfg.lle_modules.end(), m.first) != host.cfg.lle_modules.end();

        std::sort(gui.modules.begin(), gui.modules.end(), [](const auto &ma, const auto &mb) {
            if (ma.second == mb.second)
                return ma.first < mb.first;
            return ma.second;
        });
    }
}

void draw_settings_dialog(GuiState &gui, HostState &host) {
    const ImGuiWindowFlags settings_flags = ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    ImGui::Begin("Settings", &gui.configuration_menu.settings_dialog, settings_flags);
    const ImGuiTabBarFlags settings_tab_flags = ImGuiTabBarFlags_None;
    ImGui::BeginTabBar("SettingsTabBar", settings_tab_flags);

    // Core
    if (ImGui::BeginTabItem("Core")) {
        ImGui::PopStyleColor();
        if (!gui.modules.empty()) {
            ImGui::TextColored(GUI_COLOR_TEXT, "Module List");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your desired modules.");
            ImGui::PushItemWidth(240);
            if (ImGui::ListBoxHeader("##modules_list", static_cast<int>(gui.modules.size()), 8)) {
                for (auto &m : gui.modules) {
                    const auto module = std::find(host.cfg.lle_modules.begin(), host.cfg.lle_modules.end(), m.first);
                    const bool module_existed = (module != host.cfg.lle_modules.end());
                    if (!gui.module_search_bar.PassFilter(m.first.c_str()))
                        continue;
                    if (ImGui::Selectable(m.first.c_str(), &m.second)) {
                        if (module_existed)
                            host.cfg.lle_modules.erase(module);
                        else
                            host.cfg.lle_modules.push_back(m.first);
                    }
                }
                ImGui::ListBoxFooter();
            }
            ImGui::PopItemWidth();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT, "Modules Search");
            gui.module_search_bar.Draw("##module_search_bar", 200);
            ImGui::Spacing();
            if (ImGui::Button("Clear list")) {
                host.cfg.lle_modules.clear();
                for (auto &m : gui.modules)
                    m.second = false;
            }
            ImGui::SameLine();
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "No modules present.\nPlease install the firmware");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Put decrypted modules inside 'vs0/sys/external/'.");
        }
        if (ImGui::Button("Refresh list"))
            get_modules_list(gui, host);
        ImGui::EndTabItem();
    } else {
        ImGui::PopStyleColor();
    }

    // GPU
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    if (ImGui::BeginTabItem("GPU")) {
        ImGui::PopStyleColor();
        ImGui::Checkbox("Hardware Flip", &host.cfg.hardware_flip);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable texture flipping from GPU side.\nIt is recommended to disable this option for homebrew.");
        ImGui::EndTabItem();
    } else {
        ImGui::PopStyleColor();
    }

    // System
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    if (ImGui::BeginTabItem("System")) {
        ImGui::PopStyleColor();
        ImGui::Combo("Console Language", &host.cfg.sys_lang, LIST_SYS_LANG, SYS_LANG_COUNT, 6);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your language. \nNote that some games might not have your language.");
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Enter Button Assignment \nSelect your 'Enter' Button.");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("This is the button that is used as 'Confirm' in game dialogs. \nSome games don't use this and get default confirmation button.");
        ImGui::RadioButton("Circle", &host.cfg.sys_button, 0);
        ImGui::RadioButton("Cross", &host.cfg.sys_button, 1);
        ImGui::Spacing();
        ImGui::Checkbox("Emulated Console \nSelect your Console mode.", &host.cfg.pstv_mode);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable PS TV mode.");
        ImGui::EndTabItem();
    } else {
        ImGui::PopStyleColor();
    }

    // Emulator
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    if (ImGui::BeginTabItem("Emulator")) {
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Combo("Log Level", &host.cfg.log_level, "Trace\0Debug\0Info\0Warning\0Error\0Critical\0Off\0");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your preferred log level.");
        if (ImGui::Button("Apply Log Level"))
            logging::set_level(static_cast<spdlog::level::level_enum>(host.cfg.log_level));
        ImGui::Spacing();
        ImGui::Checkbox("Archive Log", &host.cfg.archive_log);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable Archiving Log.");
        ImGui::SameLine();
        ImGui::Checkbox("Performance overlay", &host.cfg.performance_overlay);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Display performance information on the screen as an overlay.");
        ImGui::SameLine();
        ImGui::Checkbox("Texture Cache", &host.cfg.texture_cache);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable texture cache.");
        ImGui::Separator();
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR_OPTIONS, "Emulated System Storage Folder");
        ImGui::Spacing();
        ImGui::PushItemWidth(320);
        ImGui::TextColored(GUI_COLOR_TEXT, "Current emulator folder: %s", host.cfg.pref_path.c_str());
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (ImGui::Button("Change Emulator Path"))
            change_emulator_path(gui, host);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Change Vita3K emulator path like wanted.\nNeed move folder old to new manualy.");
        if (host.cfg.pref_path != host.default_path) {
            ImGui::SameLine();
            if (ImGui::Button("Reset Path Emulator")) {
                if (host.default_path != host.pref_path) {
                    host.pref_path = host.default_path;
                    host.cfg.pref_path = host.pref_path;

                    config::serialize_config(host.cfg, host.cfg.config_path);

                    // Refresh the working paths
                    get_modules_list(gui, host);
                    refresh_game_list(gui, host);
                    LOG_INFO("Successfully restore default path for Vita3K files to: {}", host.pref_path);
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reset Vita3K emulator path to default.\nNeed move folder old to default manualy.");
        }
        ImGui::EndTabItem();
    } else {
        ImGui::PopStyleColor();
    }

    // GUI
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    if (ImGui::BeginTabItem("GUI")) {
        ImGui::PopStyleColor();
        ImGui::Checkbox("GUI Visible", &host.cfg.show_gui);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to show GUI after booting a game.");
        ImGui::SameLine();
        ImGui::Checkbox("Game Background", &host.cfg.show_game_background);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable viewing Game background.");
        ImGui::SameLine();
        ImGui::Checkbox("Discord Rich Presence", &host.cfg.discord_rich_presence);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enables Discord Rich Presence to show what game you're playing on discord");
        ImGui::Spacing();
        ImGui::SliderInt("Game Icon Size \nSelect your preferred icon size.", &host.cfg.icon_size, 32, 128);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR_OPTIONS, "Background Image");
        ImGui::Spacing();
        std::string image_button = "Add Image";
        if (gui.user_backgrounds[host.cfg.background_image]) {
            ImGui::PushItemWidth(400);
            ImGui::TextColored(GUI_COLOR_TEXT, "Current image: %s", host.cfg.background_image.c_str());
            ImGui::PopItemWidth();
            ImGui::Spacing();
            if (ImGui::Button("Reset Image")) {
                if (gui.current_background == gui.user_backgrounds[host.cfg.background_image])
                    gui.current_background = nullptr;
                host.cfg.background_image.clear();
                gui.user_backgrounds.clear();
            }
            image_button = "Change Image";
            ImGui::SameLine();
        }
        if (ImGui::Button(image_button.c_str()))
            LOG_INFO_IF(change_user_image_background(gui, host), "Succes change image: {}", host.cfg.background_image);
        if (gui.current_background) {
            ImGui::Spacing();
            ImGui::SliderFloat("Background Alpha\nSelect your preferred transparent background effect.", &host.cfg.background_alpha, 0.999f, 0.000f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("The minimum slider is opaque and the maximum is transparent.");
        }
        ImGui::EndTabItem();
    } else {
        ImGui::PopStyleColor();
    }

    // Debug
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
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
        ImGui::Checkbox("Log Uniforms", &host.cfg.log_uniforms);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Log shader uniform names and values.");
        ImGui::Checkbox("Save color surfaces", &host.cfg.color_surface_debug);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Save color surfaces to files.");
        ImGui::EndTabItem();
    } else {
        ImGui::PopStyleColor();
    }

    if (host.cfg.overwrite_config)
        config::serialize_config(host.cfg, host.cfg.config_path);

    ImGui::EndTabBar();
    ImGui::End();
}

} // namespace gui
