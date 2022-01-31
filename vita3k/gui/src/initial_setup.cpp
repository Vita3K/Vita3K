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

#include "private.h"

#include <gui/functions.h>

#include <util/string_utils.h>

#include <nfd.h>

namespace gui {

enum InitialSetup {
    SELECT_LANGUAGE,
    SELECT_PREF_PATH,
    INSTALL_FIRMWARE,
    SELECT_INTERFACE_SETTINGS,
    FINISHED
};

static InitialSetup setup = SELECT_LANGUAGE;
static std::string title_str;

void draw_initial_setup(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto WINDOW_SIZE = ImVec2(756.f * SCALE.x, 418.f * SCALE.y);
    const auto SELECT_SIZE = 72.f * SCALE.y;
    const auto BUTTON_SIZE = ImVec2(154.f * SCALE.x, 52.f * SCALE.y);
    const auto BIG_BUTTON_SIZE = ImVec2(324.f * SCALE.x, 48.f * SCALE.y);
    const auto BIG_BUTTON_POS = ImVec2((WINDOW_SIZE.x / 2.f) - (BIG_BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BIG_BUTTON_SIZE.y - (20.f * SCALE.y));

    auto lang = gui.lang.initial_setup;
    const auto back = !lang["back"].empty() ? lang["back"].c_str() : "Back";
    const auto completed_setup = !lang["completed_setup"].empty() ? lang["completed_setup"].c_str() : "You have now completed initial setup.\nYour Vita3K system is ready !";
    const auto select_language = !lang["select_language"].empty() ? lang["select_language"].c_str() : "Select a language";
    const auto next = !lang["next"].empty() ? lang["next"].c_str() : "Next";

    const auto is_default_path = host.cfg.pref_path == host.default_path;
    const auto FW_PATH{ fs::path(host.pref_path) / "vs0" };
    const auto FW_INSTALLED = fs::exists(FW_PATH) && !fs::is_empty(FW_PATH);
    const auto FW_FONT_PATH{ fs::path(host.pref_path) / "sa0" };
    const auto FW_FONT_INSTALLED = fs::exists(FW_FONT_PATH) && !fs::is_empty(FW_FONT_PATH);

    ImGui::PushFont(gui.vita_font);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##initial_setup", &host.cfg.initial_setup, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0), display_size, IM_COL32(112.f, 124.f, 246.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);
    ImGui::SetNextWindowPos(ImVec2(98.f * SCALE.x, 30 * SCALE.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.f * SCALE.x);
    ImGui::PushStyleColor(ImGuiCol_Border, GUI_COLOR_TEXT);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::BeginChild("##window_box", WINDOW_SIZE, true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.3f * RES_SCALE.x);
    const auto SELECT_COLOR = ImVec4(0.23f, 0.68f, 0.95f, 0.60f);
    const auto SELECT_COLOR_HOVERED = ImVec4(0.23f, 0.68f, 0.99f, 0.80f);
    const auto SELECT_COLOR_ACTIVE = ImVec4(0.23f, 0.68f, 1.f, 1.f);

    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(title_str.c_str()).x / 2.f), (47 * SCALE.y) - (ImGui::GetFontSize() / 2.f)));
    ImGui::Text("%s", title_str.c_str());
    ImGui::SetCursorPosY(94.f * SCALE.y);
    ImGui::Separator();
    switch (setup) {
    case SELECT_LANGUAGE:
        title_str = select_language;
        ImGui::SetNextWindowPos(ImVec2(198.f * SCALE.x, 126.f * SCALE.y), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
        ImGui::BeginChild("##lang_list", ImVec2(WINDOW_SIZE.x - (200.f * SCALE.x), WINDOW_SIZE.y - (108.f * SCALE.y)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Columns(3, nullptr, false);
        ImGui::SetColumnWidth(0, 96.f * SCALE.x);
        ImGui::SetColumnWidth(1, 30.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Header, SELECT_COLOR);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, SELECT_COLOR_HOVERED);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, SELECT_COLOR_ACTIVE);
        for (const auto sys_lang : LIST_SYS_LANG) {
            ImGui::PushID(sys_lang.first);
            // Empty Padding
            ImGui::NextColumn();
            const auto is_current_lang = host.cfg.sys_lang == sys_lang.first;
            if (ImGui::Selectable(is_current_lang ? "V" : "##lang_list", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(WINDOW_SIZE.x, SELECT_SIZE))) {
                host.cfg.sys_lang = sys_lang.first;
                init_lang(gui, host);
            }
            ImGui::NextColumn();
            ImGui::Selectable(sys_lang.second.c_str(), false, ImGuiSelectableFlags_None, ImVec2(WINDOW_SIZE.x, SELECT_SIZE));
            ImGui::Separator();
            ImGui::NextColumn();
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::Columns(1);
        ImGui::EndChild();
        ImGui::PopStyleVar();
        break;
    case SELECT_PREF_PATH:
        title_str = "Select a pref path.";
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize("Current emulator folder").x / 2.f), (WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize()));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Current emulator path");
        ImGui::Spacing();
        ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(host.cfg.pref_path.c_str()).x / 2.f));
        ImGui::TextWrapped("%s", host.cfg.pref_path.c_str());
        ImGui::SetCursorPos(!is_default_path ? ImVec2((WINDOW_SIZE.x / 2.f) - BIG_BUTTON_SIZE.x - (20.f * SCALE.x), BIG_BUTTON_POS.y) : BIG_BUTTON_POS);
        if (ImGui::Button("Change Emulator Path", BIG_BUTTON_SIZE)) {
            nfdchar_t *emulator_path = nullptr;
            nfdresult_t result = NFD_PickFolder(nullptr, &emulator_path);

            if ((result == NFD_OKAY) && (string_utils::utf_to_wide(emulator_path) != host.pref_path)) {
                host.pref_path = string_utils::utf_to_wide(emulator_path) + L'/';
                host.cfg.pref_path = emulator_path;
            }
        }
        if (!is_default_path) {
            ImGui::SameLine(0, 40.f * SCALE.x);
            if (ImGui::Button("Reset Emulator Path", BIG_BUTTON_SIZE)) {
                if (string_utils::utf_to_wide(host.default_path) != host.pref_path) {
                    host.pref_path = string_utils::utf_to_wide(host.default_path);
                    host.cfg.pref_path = host.default_path;
                }
            }
        }
        break;
    case INSTALL_FIRMWARE:
        title_str = "Install Firmware.";
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize("Installing both firmware files is highly recommended.").x / 2.f), (WINDOW_SIZE.y / 2.f) - (ImGui::GetFontSize() * 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Installing both firmware files is highly recommended.");
        ImGui::Spacing();
        if (ImGui::Button("Download Firmware", BIG_BUTTON_SIZE))
            open_path("https://www.playstation.com/en-us/support/hardware/psvita/system-software/");
        ImGui::SameLine(0, 20.f * SCALE.x);
        ImGui::Text("Installed: %s", FW_INSTALLED ? "V" : "X");
        ImGui::Spacing();
        if (ImGui::Button("Download Font Package", BIG_BUTTON_SIZE))
            open_path("https://bit.ly/2P2rb0r");
        ImGui::SameLine(0, 20.f * SCALE.x);
        ImGui::Text("Installed: %s", FW_FONT_INSTALLED ? "V" : "X");
        ImGui::SetCursorPos(BIG_BUTTON_POS);
        if (ImGui::Button("Install Firmware File", BIG_BUTTON_SIZE))
            gui.file_menu.firmware_install_dialog = true;
        if (gui.file_menu.firmware_install_dialog) {
            ImGui::PushFont(gui.monospaced_font);
            draw_firmware_install_dialog(gui, host);
            ImGui::PopFont();
        }
        break;
    case SELECT_INTERFACE_SETTINGS:
        title_str = "Select interface settings.";
        ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize());
        ImGui::Checkbox("Info Bar Visible", &host.cfg.show_info_bar);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to show info bar inside app selector.\nInfo bar is clock, battery level and notification center");
        ImGui::SameLine();
        ImGui::Checkbox("Live Area App Screen", &host.cfg.show_live_area_screen);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to open Live Area by default when clicking on an application.\nIf disabled, right click on an application to open it.");
        ImGui::Spacing();
        ImGui::Checkbox("Grid Mode", &host.cfg.apps_list_grid);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to set the app list to grid mode like of PsVita.");
        if (!host.cfg.apps_list_grid) {
            ImGui::Spacing();
            ImGui::SliderInt("App Icon Size", &host.cfg.icon_size, 64, 128);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred icon size.");
        }
        break;
    case FINISHED:
        title_str = "Completed.";
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(completed_setup).x / 2.f), (WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize()));
        ImGui::Text("%s", completed_setup);
        ImGui::SetCursorPos(BIG_BUTTON_POS);
        if (ImGui::Button("Ok", BIG_BUTTON_SIZE))
            host.cfg.initial_setup = true;
        break;
    default: break;
    }

    ImGui::SetWindowFontScale(1.f);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Draw Button
    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, display_size.y - BUTTON_SIZE.y - (14.f * SCALE.y)));
    if ((setup > SELECT_LANGUAGE) && ImGui::Button(back, BUTTON_SIZE))
        setup = (InitialSetup)(setup - 1);
    ImGui::SetCursorPos(ImVec2(display_size.x - BUTTON_SIZE.x - (14.f * SCALE.x), display_size.y - BUTTON_SIZE.y - (14.f * SCALE.y)));
    if ((setup < FINISHED) && ImGui::Button(next, BUTTON_SIZE))
        setup = (InitialSetup)(setup + 1);
    ImGui::SetWindowFontScale(1.f);

    ImGui::PopStyleVar(3);

    ImGui::SetWindowFontScale(1.f);
    ImGui::End();
    ImGui::PopFont();
}

} // namespace gui
