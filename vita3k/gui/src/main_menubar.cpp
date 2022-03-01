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

#include <gui/functions.h>

#include <util/string_utils.h>

#include "private.h"

namespace gui {

static void draw_file_menu(GuiState &gui, HostState &host) {
    auto lang = gui.lang.main_menubar.file;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        if (ImGui::MenuItem(lang["open_pref_path"].c_str()))
            open_path(string_utils::wide_to_utf(host.pref_path));
        ImGui::Separator();
        ImGui::MenuItem(lang["install_firmware"].c_str(), nullptr, &gui.file_menu.firmware_install_dialog);
        ImGui::MenuItem(lang["install_pkg"].c_str(), nullptr, &gui.file_menu.pkg_install_dialog);
        ImGui::MenuItem(lang["install_zip"].c_str(), nullptr, &gui.file_menu.archive_install_dialog);
        ImGui::MenuItem(lang["install_license"].c_str(), nullptr, &gui.file_menu.license_install_dialog);
        ImGui::EndMenu();
    }
}

static void draw_emulation_menu(GuiState &gui, HostState &host) {
    auto lang = gui.lang.main_menubar.emulation;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        if (ImGui::BeginMenu(lang["last_apps_used"].c_str())) {
            if (!gui.time_apps[host.io.user_id].empty()) {
                for (auto i = 0; i < std::min(14, int32_t(gui.time_apps[host.io.user_id].size())); i++) {
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    const auto time_app = gui.time_apps[host.io.user_id][i];
                    const auto app_index = get_app_index(gui, time_app.app);
                    if ((app_index != gui.app_selector.user_apps.end()) && ImGui::MenuItem(app_index->title.c_str(), time_app.app.c_str(), false))
                        pre_load_app(gui, host, host.cfg.show_live_area_screen, time_app.app);
                    ImGui::PopStyleColor();
                }
            } else
                ImGui::MenuItem("Empty", nullptr, false, false);
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
}

static void draw_debug_menu(DebugMenuState &state) {
    if (ImGui::BeginMenu("Debug")) {
        ImGui::MenuItem("Threads", nullptr, &state.threads_dialog);
        ImGui::MenuItem("Semaphores", nullptr, &state.semaphores_dialog);
        ImGui::MenuItem("Mutexes", nullptr, &state.mutexes_dialog);
        ImGui::MenuItem("Lightweight Mutexes", nullptr, &state.lwmutexes_dialog);
        ImGui::MenuItem("Condition Variables", nullptr, &state.condvars_dialog);
        ImGui::MenuItem("Lightweight Condition Variables", nullptr, &state.lwcondvars_dialog);
        ImGui::MenuItem("Event Flags", nullptr, &state.eventflags_dialog);
        ImGui::MenuItem("Memory Allocations", nullptr, &state.allocations_dialog);
        ImGui::MenuItem("Disassembly", nullptr, &state.disassembly_dialog);
        ImGui::EndMenu();
    }
}

static void draw_config_menu(GuiState &gui, HostState &host) {
    auto lang = gui.lang.main_menubar.configuration;
    const auto CUSTOM_CONFIG_PATH{ fs::path(host.base_path) / "config" / fmt::format("config_{}.xml", host.io.app_path) };
    auto &settings_dialog = !host.io.app_path.empty() && fs::exists(CUSTOM_CONFIG_PATH) ? gui.configuration_menu.custom_settings_dialog : gui.configuration_menu.settings_dialog;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        if (ImGui::MenuItem(lang["settings"].c_str(), nullptr, &settings_dialog))
            init_config(gui, host, host.io.app_path);
        if (ImGui::MenuItem(lang["user_management"].c_str(), nullptr, &gui.live_area.user_management, (!gui.live_area.user_management && host.io.title_id.empty()))) {
            gui.live_area.app_selector = false;
            gui.live_area.information_bar = true;
        }
        ImGui::EndMenu();
    }
}

static void draw_controls_menu(GuiState &gui) {
    auto lang = gui.lang.main_menubar.controls;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        ImGui::MenuItem(lang["keyboard_controls"].c_str(), nullptr, &gui.controls_menu.controls_dialog);
        ImGui::MenuItem(lang["controllers"].c_str(), nullptr, &gui.controls_menu.controllers_dialog);
        ImGui::EndMenu();
    }
}

static void draw_help_menu(GuiState &gui) {
    auto lang = gui.lang.main_menubar.help;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        ImGui::MenuItem(lang["about"].c_str(), nullptr, &gui.help_menu.about_dialog);
        ImGui::MenuItem(lang["welcome"].c_str(), nullptr, &gui.help_menu.welcome_dialog);
        ImGui::EndMenu();
    }
}

void draw_main_menu_bar(GuiState &gui, HostState &host) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::SetWindowFontScale(ImGui::GetIO().DisplaySize.x / host.res_width_dpi_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);

        draw_file_menu(gui, host);
        draw_emulation_menu(gui, host);
        draw_debug_menu(gui.debug_menu);
        draw_config_menu(gui, host);
        draw_controls_menu(gui);
        draw_help_menu(gui);

        ImGui::PopStyleColor();
        ImGui::EndMainMenuBar();
    }
}

} // namespace gui
