// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <config/state.h>
#include <gui/functions.h>
#include <io/state.h>

#include "private.h"

namespace gui {

static void draw_file_menu(GuiState &gui, EmuEnvState &emuenv) {
    const auto textures_path{ emuenv.shared_path / "textures" };

    auto &lang = gui.lang.main_menubar.file;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        if (ImGui::MenuItem(lang["open_pref_path"].c_str()))
            open_path(emuenv.pref_path.string());
        if (ImGui::MenuItem(lang["open_textures_path"].c_str())) {
            if (!fs::exists(textures_path)) {
                fs::create_directories(textures_path);
                fs::create_directories(textures_path / "export");
                fs::create_directories(textures_path / "import");
            }
            open_path(textures_path.string());
        }
        ImGui::Separator();
        ImGui::MenuItem(lang["install_firmware"].c_str(), nullptr, &gui.file_menu.firmware_install_dialog);
        ImGui::MenuItem(lang["install_pkg"].c_str(), nullptr, &gui.file_menu.pkg_install_dialog);
        ImGui::MenuItem(lang["install_zip"].c_str(), nullptr, &gui.file_menu.archive_install_dialog);
        ImGui::MenuItem(lang["install_license"].c_str(), nullptr, &gui.file_menu.license_install_dialog);
        ImGui::Separator();
        if (ImGui::MenuItem(lang["exit"].c_str()))
            exit(0);
        ImGui::EndMenu();
    }
}

static void draw_emulation_menu(GuiState &gui, EmuEnvState &emuenv) {
    auto &lang = gui.lang.main_menubar.emulation;
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const ImVec2 ICON_SIZE(56.f * SCALE.x, 56.f * SCALE.y);
    const auto PADDING = 10.f * SCALE.x;

    const auto draw_app = [&](const App &app) {
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        ImGui::PushID(app.path.c_str());
        const auto APP_ICON = get_app_icon(gui, app.path);
        if (!APP_ICON->first.empty()) {
            const auto POS_MIN = ImGui::GetCursorScreenPos();
            const ImVec2 POS_MAX(POS_MIN.x + ICON_SIZE.x, POS_MIN.y + ICON_SIZE.y);
            ImGui::GetWindowDrawList()->AddImageRounded(APP_ICON->second, POS_MIN, POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x * SCALE.x, ImDrawFlags_RoundCornersAll);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ICON_SIZE.x + PADDING);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        if (ImGui::Selectable(app.title.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, ICON_SIZE.y)))
            pre_load_app(gui, emuenv, emuenv.cfg.show_live_area_screen, app.title_id);
        ImGui::PopStyleVar();
        ImGui::PopID();
        ImGui::PopStyleColor();
    };

    if (ImGui::BeginMenu(lang["title"].c_str())) {
        const auto app_list_is_empty = gui.time_apps[emuenv.io.user_id].empty();
        ImGui::SetNextWindowSize(ImVec2(!app_list_is_empty ? 480.f * SCALE.x : 0.f, 0.f));
        ImGui::SetWindowFontScale(RES_SCALE.x);
        if (ImGui::BeginMenu(lang["last_apps_used"].c_str())) {
            if (!app_list_is_empty) {
                for (size_t i = 0; i < std::min<size_t>(8, gui.time_apps[emuenv.io.user_id].size()); i++) {
                    const auto &time_app = gui.time_apps[emuenv.io.user_id][i];
                    const auto app_index = get_app_index(gui, time_app.app);
                    if (app_index)
                        draw_app(*app_index);
                }
            } else
                ImGui::MenuItem("Empty", nullptr, false, false);
            ImGui::EndMenu();
        }
        if (!emuenv.cfg.display_system_apps) {
            ImGui::Separator();
            for (const auto &app : gui.app_selector.sys_apps)
                draw_app(app);
        }
        ImGui::EndMenu();
    }
}

static void draw_debug_menu(GuiState &gui, DebugMenuState &state) {
    auto &lang = gui.lang.main_menubar.debug;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        ImGui::MenuItem(lang["threads"].c_str(), nullptr, &state.threads_dialog);
        ImGui::MenuItem(lang["semaphores"].c_str(), nullptr, &state.semaphores_dialog);
        ImGui::MenuItem(lang["mutexes"].c_str(), nullptr, &state.mutexes_dialog);
        ImGui::MenuItem(lang["lightweight_mutexes"].c_str(), nullptr, &state.lwmutexes_dialog);
        ImGui::MenuItem(lang["condition_variables"].c_str(), nullptr, &state.condvars_dialog);
        ImGui::MenuItem(lang["lightweight_condition_variables"].c_str(), nullptr, &state.lwcondvars_dialog);
        ImGui::MenuItem(lang["event_flags"].c_str(), nullptr, &state.eventflags_dialog);
        ImGui::MenuItem(lang["memory_allocations"].c_str(), nullptr, &state.allocations_dialog);
        ImGui::MenuItem(lang["disassembly"].c_str(), nullptr, &state.disassembly_dialog);
        ImGui::EndMenu();
    }
}

static void draw_config_menu(GuiState &gui, EmuEnvState &emuenv) {
    auto &lang = gui.lang.main_menubar.configuration;
    const auto CUSTOM_CONFIG_PATH{ emuenv.config_path / "config" / fmt::format("config_{}.xml", emuenv.io.app_path) };
    auto &settings_dialog = !emuenv.io.app_path.empty() && fs::exists(CUSTOM_CONFIG_PATH) ? gui.configuration_menu.custom_settings_dialog : gui.configuration_menu.settings_dialog;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        if (ImGui::MenuItem(gui.lang.settings_dialog.main_window["title"].c_str(), nullptr, &settings_dialog))
            init_config(gui, emuenv, emuenv.io.app_path);
        if (ImGui::MenuItem(lang["user_management"].c_str(), nullptr, &gui.vita_area.user_management, (!gui.vita_area.user_management && emuenv.io.title_id.empty())))
            init_user_management(gui, emuenv);
        ImGui::EndMenu();
    }
}

static void draw_controls_menu(GuiState &gui) {
    auto &lang = gui.lang.main_menubar.controls;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        ImGui::MenuItem(lang["keyboard_controls"].c_str(), nullptr, &gui.controls_menu.controls_dialog);
        ImGui::MenuItem(gui.lang.controllers["title"].c_str(), nullptr, &gui.controls_menu.controllers_dialog);
        ImGui::EndMenu();
    }
}

static void draw_help_menu(GuiState &gui) {
    auto &lang = gui.lang.main_menubar.help;
    if (ImGui::BeginMenu(lang["title"].c_str())) {
        ImGui::MenuItem(gui.lang.about["title"].c_str(), nullptr, &gui.help_menu.about_dialog);
        if (ImGui::MenuItem(gui.lang.vita3k_update["title"].c_str(), nullptr, &gui.help_menu.vita3k_update))
            init_vita3k_update(gui);
        ImGui::MenuItem(lang["welcome"].c_str(), nullptr, &gui.help_menu.welcome_dialog);
        ImGui::EndMenu();
    }
}

void draw_main_menu_bar(GuiState &gui, EmuEnvState &emuenv) {
    if (ImGui::BeginMainMenuBar()) {
        const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
        const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);

        ImGui::SetWindowFontScale(RES_SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);

        draw_file_menu(gui, emuenv);
        draw_emulation_menu(gui, emuenv);
        draw_debug_menu(gui, gui.debug_menu);
        draw_config_menu(gui, emuenv);
        draw_controls_menu(gui);
        draw_help_menu(gui);

        ImGui::PopStyleColor();
        ImGui::EndMainMenuBar();
    }
}

} // namespace gui
