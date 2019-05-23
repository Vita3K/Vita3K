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
#include <config.h>
#include <gui/functions.h>

#include <host/functions.h>
#include <host/state.h>
#include <misc/cpp/imgui_stdlib.h>
#include <util/fs.h>
#include <util/log.h>

namespace gui {

bool change_pref_location(const std::string &input_path, const std::string &current_path) {
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

void draw_settings_dialog(HostState &host) {
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    ImGui::Begin("Settings", &host.gui.configuration_menu.settings_dialog, ImGuiWindowFlags_AlwaysAutoResize);
    ImGuiTabBarFlags settings_tab_flags = ImGuiTabBarFlags_None;
    ImGui::BeginTabBar("SettingsTabBar", settings_tab_flags);

    // System
    if (ImGui::BeginTabItem("System")) {
        ImGui::PopStyleColor();
        static const char *list_sys_lang[] = {
            "Japanese", "American English", "French", "Spanish", "German", "Italian", "Dutch", "Portugal Portuguese", "Russian", "Korean",
            "Traditional Chinese", "Simplified Chinese", "Finnish", "Swedish", "Danish", "Norwegian", "Polish", "Brazil Portuguese", "British English", "Turkish"
        };
        ImGui::Combo("Console Language", &host.cfg.sys_lang, list_sys_lang, IM_ARRAYSIZE(list_sys_lang), 10);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select your language. \nNote that some games might not have your language.");
        ImGui::Spacing();
        ImGui::Text("Enter Button Assignment \nSelect your 'Enter' Button.");
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
        ImGui::Checkbox("Archive Log", &host.cfg.archive_log);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to enable Archiving Log.");
        ImGui::SameLine();
        ImGui::Checkbox("Texture Cache", &host.cfg.texture_cache);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Uncheck the box to disable texture cache.");
        ImGui::Spacing();
        ImGui::PushItemWidth(400);
        ImGui::InputTextWithHint("Set emulated system storage folder.", "Write your path folder here", &host.cfg.pref_path);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Set the path to the folder here. \nPress \"Apply\" when finished to move to the new folder. \nThis cannot be undone.");
        if (ImGui::Button("Apply")) {
            if (!host.cfg.pref_path.empty() && host.cfg.pref_path != host.pref_path) {
                if (change_pref_location(host.cfg.pref_path, host.pref_path)) {
                    host.pref_path = host.cfg.pref_path;

                    // Refresh the working paths
                    config::serialize(host.cfg, host.cfg.config_path);
                    LOG_INFO_IF(clear_and_refresh_game_list(host), "Successfully moved Vita3K files to: {}", host.pref_path);
                }
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("After pressing, restart Vita3K to fully apply changes.");
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
        ImGui::Spacing();
        ImGui::SliderInt("Game Icon Size \nSelect your preferred icon size.", &host.cfg.icon_size, 16, 128);
        ImGui::Spacing();
        ImGui::PushItemWidth(400);
        ImGui::InputTextWithHint("Set background image", "Add your path to the image here", &host.cfg.background_image);
        ImGui::PopItemWidth();
        if (ImGui::Button("Apply Change Image")) {
            if (!host.gui.user_backgrounds[host.cfg.background_image])
                init_background(host, host.cfg.background_image);
            else if (host.gui.user_backgrounds[host.cfg.background_image])
                host.gui.current_background = host.gui.user_backgrounds[host.cfg.background_image];
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Background")) {
            host.cfg.background_image.clear();
            host.gui.user_backgrounds.clear();
            host.gui.current_background = 0;
        }
        ImGui::Spacing();
        if (host.gui.current_background) {
            ImGui::SliderFloat("Background Alpha \nSelect your preferred transparent background effect.", &host.cfg.background_alpha, 0.999f, 0.000f);
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
        ImGui::EndTabItem();
    } else {
        ImGui::PopStyleColor();
    }

    if (host.cfg.overwrite_config)
        config::serialize(host.cfg, host.cfg.config_path);

    ImGui::EndTabBar();
    ImGui::End();
}

} // namespace gui
