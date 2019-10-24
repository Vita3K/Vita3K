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

#include <gui/functions.h>

#include <config/config.h>
#include <config/functions.h>
#include <gui/state.h>

#include <host/state.h>
#include <misc/cpp/imgui_stdlib.h>
#include <util/fs.h>
#include <util/log.h>

#include <algorithm>

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

static bool clear_and_refresh_game_list(HostState &host, GuiState &gui) {
    if (gui.game_selector.games.empty())
        return false;

    gui.game_selector.games.clear();
    if (!gui.game_selector.games.empty())
        return false;

    get_game_titles(gui, host);
    return true;
}

void get_modules_list(GuiState &gui, HostState &host)
{
    gui.modules_list.clear();

    const auto modules_path{ fs::path(host.pref_path) / "vs0/sys/external/" };
    if (!modules_path.empty()) {
        fs::recursive_directory_iterator end;
        const std::string ext = ".suprx";
        for (fs::recursive_directory_iterator m(modules_path); m != end; ++m) {
            const fs::path lle = *m;
            if (m->path().extension() == ext)
                gui.modules_list.push_back(lle.filename().replace_extension().string());
        }
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
        if (!gui.modules_list.empty()) {
            ImGui::TextColored(GUI_COLOR_TEXT, "Module List");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your desired modules.");
            ImGui::PushItemWidth(240);
            static std::vector<bool> module_select(gui.modules_list.size());
            if (ImGui::ListBoxHeader("##modules_list", static_cast<int>(gui.modules_list.size()), 6)) {
                for (int m = 0; m < gui.modules_list.size(); m++) {
                    auto modules = std::find(host.cfg.lle_modules.begin(), host.cfg.lle_modules.end(), gui.modules_list[m]);
                    if (modules != host.cfg.lle_modules.end() && !module_select[m])
                        module_select[m] = true;
                    if (!gui.module_search_bar.PassFilter(gui.modules_list[m].c_str()))
                        continue;
                    if (ImGui::Selectable(gui.modules_list[m].c_str(), module_select[m]) && modules == host.cfg.lle_modules.end())
                        host.cfg.lle_modules.push_back(gui.modules_list[m]);
                    else if (!module_select[m] && modules != host.cfg.lle_modules.end())
                        host.cfg.lle_modules.erase(modules);
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
                std::fill(module_select.begin(), module_select.end(), false);
            }
        } else {
            ImGui::TextColored(GUI_COLOR_TEXT, "No modules present.\nPlease dump decrypted modules from your PS Vita.");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Put decrypted modules inside 'vs0/sys/external/'.");
        }
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
        ImGui::Spacing();
        ImGui::PushItemWidth(320);
        ImGui::InputTextWithHint("Set emulated system storage folder.", "Write your path folder here", &host.cfg.pref_path);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Set the path to the folder here. \nPress \"Apply\" when finished to move to the new folder. \nWARNING: This cannot be undone.");
        if (ImGui::Button("Apply New Path") && !host.cfg.pref_path.empty()) {
            if (host.cfg.pref_path.back() != '/')
                host.cfg.pref_path += '/';
            if (host.cfg.pref_path != host.pref_path) {
                if (change_pref_location(host.cfg.pref_path, host.pref_path)) {
                    host.pref_path = host.cfg.pref_path;

                    // Refresh the working paths
                    config::serialize_config(host.cfg, host.cfg.config_path);
                    LOG_INFO_IF(clear_and_refresh_game_list(host, gui), "Successfully moved Vita3K files to: {}", host.pref_path);
                }
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("After pressing, restart Vita3K to fully apply changes.");
        ImGui::SameLine();
        if (ImGui::Button("Reset Configuration")) {
            host.cfg = Config{};

            LOG_INFO("Resetted Vita3K configuration and config file to default values.");
            if (host.cfg.pref_path != host.pref_path) {
                if (change_pref_location(host.cfg.pref_path, host.pref_path)) {
                    host.pref_path = host.cfg.pref_path;

                    // Refresh the working paths
                    config::serialize_config(host.cfg, host.cfg.config_path);
                    LOG_INFO_IF(clear_and_refresh_game_list(host, gui), "Successfully moved Vita3K files to: {}", host.pref_path);
                }
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Reset Vita3K's configuration to the default values. \nAfter pressing, restart Vita3K to fully apply. \nWARNING: This cannot be undone.");
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
        ImGui::SliderInt("Game Icon Size \nSelect your preferred icon size.", &host.cfg.icon_size, 16, 128);
        ImGui::Spacing();
        ImGui::PushItemWidth(400);
        ImGui::InputTextWithHint("Set background image", "Add your path to the image here", &host.cfg.background_image);
        ImGui::PopItemWidth();
        if (ImGui::Button("Apply Change Image")) {
            if (!gui.user_backgrounds[host.cfg.background_image])
                init_background(gui, host.cfg.background_image);
            else if (gui.user_backgrounds[host.cfg.background_image])
                gui.current_background = gui.user_backgrounds[host.cfg.background_image];
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Background")) {
            host.cfg.background_image.clear();
            gui.user_backgrounds.clear();
            gui.current_background = nullptr;
        }
        ImGui::Spacing();
        if (gui.current_background) {
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
