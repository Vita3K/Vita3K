// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <config/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.hpp>
#include <lang/functions.h>

#include <util/string_utils.h>

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

void get_firmware_file(EmuEnvState &emuenv) {
    if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_JAPANESE)
        open_path("https://www.playstation.com/ja-jp/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_ENGLISH_US)
        open_path("https://www.playstation.com/en-us/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_FRENCH)
        open_path("https://www.playstation.com/fr-fr/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_SPANISH)
        open_path("https://www.playstation.com/es-es/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_GERMAN)
        open_path("https://www.playstation.com/de-de/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_ITALIAN)
        open_path("https://www.playstation.com/it-it/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_DUTCH)
        open_path("https://www.playstation.com/nl-nl/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT)
        open_path("https://www.playstation.com/pt-pt/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_RUSSIAN)
        open_path("https://www.playstation.com/ru-ru/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_KOREAN)
        open_path("https://www.playstation.com/ko-kr/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_T)
        open_path("https://www.playstation.com/zh-hant-hk/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_CHINESE_S)
        open_path("https://www.playstation.com/zh-hans-cn/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_FINNISH)
        open_path("https://www.playstation.com/fi-fi/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_SWEDISH)
        open_path("https://www.playstation.com/sv-sv/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_DANISH)
        open_path("https://www.playstation.com/da-dk/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_NORWEGIAN)
        open_path("https://www.playstation.com/no-no/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_POLISH)
        open_path("https://www.playstation.com/pl-pl/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR)
        open_path("https://www.playstation.com/pt-br/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_ENGLISH_GB)
        open_path("https://www.playstation.com/en-gb/support/hardware/psvita/system-software");
    else if (emuenv.cfg.sys_lang == SCE_SYSTEM_PARAM_LANG_TURKISH)
        open_path("https://www.playstation.com/tr-tr/support/hardware/psvita/system-software");
}

void draw_initial_setup(GuiState &gui, EmuEnvState &emuenv) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const auto WINDOW_SIZE = ImVec2(756.f * SCALE.x, 418.f * SCALE.y);
    const auto SELECT_SIZE = 72.f * SCALE.y;
    const auto BUTTON_SIZE = ImVec2(154.f * SCALE.x, 52.f * SCALE.y);
    const auto BIG_BUTTON_SIZE = ImVec2(324.f * SCALE.x, 48.f * SCALE.y);
    const auto BIG_BUTTON_POS = ImVec2((WINDOW_SIZE.x / 2.f) - (BIG_BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BIG_BUTTON_SIZE.y - (20.f * SCALE.y));

    auto lang = gui.lang.initial_setup;
    auto common = emuenv.common_dialog.lang.common;
    const auto completed_setup = lang["completed_setup"].c_str();

    const auto is_default_path = emuenv.cfg.pref_path == emuenv.default_path;
    const auto FW_PATH{ emuenv.pref_path / "vs0" };
    const auto FW_INSTALLED = fs::exists(FW_PATH) && !fs::is_empty(FW_PATH);
    const auto FW_FONT_PATH{ emuenv.pref_path / "sa0" };
    const auto FW_FONT_INSTALLED = fs::exists(FW_FONT_PATH) && !fs::is_empty(FW_FONT_PATH);

    ImGui::PushFont(gui.vita_font);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##initial_setup", &emuenv.cfg.initial_setup, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
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
        title_str = lang["select_language"];
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
        for (const auto &sys_lang : LIST_SYS_LANG) {
            ImGui::PushID(sys_lang.first);
            // Empty Padding
            ImGui::NextColumn();
            const auto is_current_lang = emuenv.cfg.sys_lang == sys_lang.first;
            if (ImGui::Selectable(is_current_lang ? "V" : "##lang_list", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(WINDOW_SIZE.x, SELECT_SIZE))) {
                emuenv.cfg.sys_lang = sys_lang.first;
                lang::init_lang(gui.lang, emuenv);
            }
            ImGui::NextColumn();
            ImGui::Selectable(sys_lang.second.c_str(), false, ImGuiSelectableFlags_None, ImVec2(WINDOW_SIZE.x, SELECT_SIZE));
            ImGui::Separator();
            ImGui::NextColumn();
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::Columns(1);
        ImGui::EndChild();
        ImGui::PopStyleVar();
        break;
    case SELECT_PREF_PATH:
        title_str = lang["select_pref_path"];
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize("Current emulator folder").x / 2.f), (WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize()));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Current emulator path");
        ImGui::Spacing();
        ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(emuenv.cfg.pref_path.c_str()).x / 2.f));
        ImGui::TextWrapped("%s", emuenv.cfg.pref_path.c_str());
        ImGui::SetCursorPos(!is_default_path ? ImVec2((WINDOW_SIZE.x / 2.f) - BIG_BUTTON_SIZE.x - (20.f * SCALE.x), BIG_BUTTON_POS.y) : BIG_BUTTON_POS);
        if (ImGui::Button("Change Emulator Path", BIG_BUTTON_SIZE)) {
            std::filesystem::path emulator_path = "";
            host::dialog::filesystem::Result result = host::dialog::filesystem::pick_folder(emulator_path);

            if ((result == host::dialog::filesystem::Result::SUCCESS) && (emulator_path.wstring() != emuenv.pref_path)) {
                emuenv.pref_path = emulator_path.wstring() + L'/';
                emuenv.cfg.pref_path = emulator_path.string();
            }
        }
        if (!is_default_path) {
            ImGui::SameLine(0, 40.f * SCALE.x);
            if (ImGui::Button("Reset Emulator Path", BIG_BUTTON_SIZE)) {
                if (emuenv.default_path != emuenv.pref_path) {
                    emuenv.pref_path = emuenv.default_path;
                    emuenv.cfg.pref_path = emuenv.default_path.string();
                }
            }
        }
        break;
    case INSTALL_FIRMWARE:
        title_str = lang["install_firmware"];
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(lang["install_highly_recommended"].c_str()).x / 2.f), (WINDOW_SIZE.y / 2.f) - (ImGui::GetFontSize() * 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["install_highly_recommended"].c_str());
        ImGui::Spacing();
        if (ImGui::Button(lang["download_firmware"].c_str(), BIG_BUTTON_SIZE))
            get_firmware_file(emuenv);
        ImGui::SameLine(0, 20.f * SCALE.x);
        ImGui::Text("%s %s", lang["installed"].c_str(), FW_INSTALLED ? "V" : "X");
        ImGui::Spacing();
        if (ImGui::Button(lang["download_font_package"].c_str(), BIG_BUTTON_SIZE))
            open_path("https://bit.ly/2P2rb0r");
        ImGui::SameLine(0, 20.f * SCALE.x);
        ImGui::Text("%s %s", lang["installed"].c_str(), FW_FONT_INSTALLED ? "V" : "X");
        ImGui::SetCursorPos(BIG_BUTTON_POS);
        if (ImGui::Button(lang["install_firmware_file"].c_str(), BIG_BUTTON_SIZE))
            gui.file_menu.firmware_install_dialog = true;
        if (gui.file_menu.firmware_install_dialog) {
            ImGui::PushFont(gui.vita_font);
            ImGui::SetWindowFontScale(RES_SCALE.x);
            draw_firmware_install_dialog(gui, emuenv);
            ImGui::PopFont();
        }
        break;
    case SELECT_INTERFACE_SETTINGS:
        title_str = lang["select_interface_settings"];
        ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize());
        ImGui::Checkbox("Info Bar Visible", &emuenv.cfg.show_info_bar);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to show info bar inside app selector.\nInfo bar is clock, battery level and notification center.");
        ImGui::SameLine();
        ImGui::Checkbox("Live Area App Screen", &emuenv.cfg.show_live_area_screen);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to open Live Area by default when clicking on an application.\nIf disabled, right click on an application to open it.");
        ImGui::Spacing();
        ImGui::Checkbox("Grid Mode", &emuenv.cfg.apps_list_grid);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Check the box to set the app list to grid mode like of PS Vita.");
        if (!emuenv.cfg.apps_list_grid) {
            ImGui::Spacing();
            ImGui::SliderInt("App Icon Size", &emuenv.cfg.icon_size, 64, 128);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Select your preferred icon size.");
        }
        break;
    case FINISHED:
        title_str = lang["completed"];
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(completed_setup).x / 2.f), (WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize()));
        ImGui::Text("%s", completed_setup);
        ImGui::SetCursorPos(BIG_BUTTON_POS);
        if (ImGui::Button(common["ok"].c_str(), BIG_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))
            emuenv.cfg.initial_setup = true;
        break;
    default: break;
    }

    ImGui::SetWindowFontScale(1.f);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Draw Button
    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, display_size.y - BUTTON_SIZE.y - (14.f * SCALE.y)));
    if ((setup > SELECT_LANGUAGE) && ImGui::Button(lang["back"].c_str(), BUTTON_SIZE) || (setup > SELECT_LANGUAGE) && ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle)))
        setup = (InitialSetup)(setup - 1);
    ImGui::SetCursorPos(ImVec2(display_size.x - BUTTON_SIZE.x - (14.f * SCALE.x), display_size.y - BUTTON_SIZE.y - (14.f * SCALE.y)));
    if ((setup < FINISHED) && ImGui::Button(lang["next"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))
        setup = (InitialSetup)(setup + 1);
    ImGui::SetWindowFontScale(1.f);

    ImGui::PopStyleVar(3);

    ImGui::SetWindowFontScale(1.f);
    ImGui::End();
    ImGui::PopFont();
}

} // namespace gui
