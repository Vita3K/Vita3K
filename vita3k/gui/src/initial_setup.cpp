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

#include "private.h"

#include <config/functions.h>
#include <config/state.h>
#include <dialog/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.h>
#include <lang/functions.h>

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
    const std::map<int, std::string> locale_id = {
        { SCE_SYSTEM_PARAM_LANG_JAPANESE, "ja-jp" },
        { SCE_SYSTEM_PARAM_LANG_ENGLISH_US, "en-us" },
        { SCE_SYSTEM_PARAM_LANG_FRENCH, "fr-fr" },
        { SCE_SYSTEM_PARAM_LANG_SPANISH, "es-es" },
        { SCE_SYSTEM_PARAM_LANG_GERMAN, "de-de" },
        { SCE_SYSTEM_PARAM_LANG_ITALIAN, "it-it" },
        { SCE_SYSTEM_PARAM_LANG_DUTCH, "nl-nl" },
        { SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT, "pt-pt" },
        { SCE_SYSTEM_PARAM_LANG_RUSSIAN, "ru-ru" },
        { SCE_SYSTEM_PARAM_LANG_KOREAN, "ko-kr" },
        { SCE_SYSTEM_PARAM_LANG_CHINESE_T, "zh-hant-hk" },
        { SCE_SYSTEM_PARAM_LANG_CHINESE_S, "zh-hans-cn" },
        { SCE_SYSTEM_PARAM_LANG_FINNISH, "fi-fi" },
        { SCE_SYSTEM_PARAM_LANG_SWEDISH, "sv-sv" },
        { SCE_SYSTEM_PARAM_LANG_DANISH, "da-dk" },
        { SCE_SYSTEM_PARAM_LANG_NORWEGIAN, "no-no" },
        { SCE_SYSTEM_PARAM_LANG_POLISH, "pl-PL" },
        { SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR, "pt-br" },
        { SCE_SYSTEM_PARAM_LANG_ENGLISH_GB, "en-gb" },
        { SCE_SYSTEM_PARAM_LANG_TURKISH, "tr-tr" },
    };
    const auto &locale = locale_id.at(emuenv.cfg.sys_lang);
    open_path(fmt::format("https://www.playstation.com/{}/support/hardware/psvita/system-software", locale));
}

void draw_initial_setup(GuiState &gui, EmuEnvState &emuenv) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const auto WINDOW_SIZE = ImVec2(756.f * SCALE.x, 418.f * SCALE.y);
    const auto SELECT_SIZE = 72.f * SCALE.y;
    const auto BUTTON_SIZE = ImVec2(186.f * SCALE.x, 52.f * SCALE.y);
    const ImVec2 BUTTON_POS(8.f * SCALE.x, display_size.y - BUTTON_SIZE.y - (6.f * SCALE.y));
    const auto BIG_BUTTON_SIZE = ImVec2(324.f * SCALE.x, 48.f * SCALE.y);
    const auto BIG_BUTTON_POS = ImVec2((WINDOW_SIZE.x / 2.f) - (BIG_BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BIG_BUTTON_SIZE.y - (20.f * SCALE.y));

    auto &lang = gui.lang.initial_setup;
    auto &common = emuenv.common_dialog.lang.common;
    const auto completed_setup = lang["completed_setup"].c_str();

    const auto is_default_path = emuenv.cfg.pref_path == emuenv.default_path;
    const auto FW_PATH{ emuenv.pref_path / "vs0" };
    const auto FW_INSTALLED = fs::exists(FW_PATH) && !fs::is_empty(FW_PATH);
    const auto FW_FONT_PATH{ emuenv.pref_path / "sa0" };
    const auto FW_FONT_INSTALLED = fs::exists(FW_FONT_PATH) && !fs::is_empty(FW_FONT_PATH);

    ImGui::PushFont(gui.vita_font);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##initial_setup", &emuenv.cfg.initial_setup, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);

    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0), display_size, IM_COL32(194.f, 207.f, 241.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);
    ImGui::SetNextWindowPos(ImVec2(98.f * SCALE.x, 30 * SCALE.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.f * SCALE.x);
    ImGui::PushStyleColor(ImGuiCol_Border, GUI_COLOR_TEXT);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::BeginChild("##window_box", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
    const auto SELECT_COLOR = ImVec4(0.23f, 0.68f, 0.95f, 0.60f);
    const auto SELECT_COLOR_HOVERED = ImVec4(0.23f, 0.68f, 0.99f, 0.80f);
    const auto SELECT_COLOR_ACTIVE = ImVec4(0.23f, 0.68f, 1.f, 1.f);

    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(title_str.c_str()).x / 2.f), (47 * SCALE.y) - (ImGui::GetFontSize() / 2.f)));
    ImGui::Text("%s", title_str.c_str());
    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    ImGui::SetCursorPosY(94.f * SCALE.y);
    ImGui::Separator();
    switch (setup) {
    case SELECT_LANGUAGE:
        title_str = lang["select_language"];
        ImGui::SetNextWindowPos(ImVec2(198.f * SCALE.x, 126.f * SCALE.y), ImGuiCond_Always);
        ImGui::BeginChild("##lang_list", ImVec2(WINDOW_SIZE.x - (200.f * SCALE.x), WINDOW_SIZE.y - (108.f * SCALE.y)), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Columns(3, nullptr, false);
        ImGui::SetColumnWidth(0, 90.f * SCALE.x);
        ImGui::SetColumnWidth(1, 34.f * SCALE.x);
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
        break;
    case SELECT_PREF_PATH:
        title_str = lang["select_pref_path"];
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(lang["current_emu_path"].c_str()).x / 2.f), (WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize()));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["current_emu_path"].c_str());
        ImGui::Spacing();
        ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(emuenv.cfg.pref_path.c_str()).x / 2.f));
        ImGui::TextWrapped("%s", emuenv.cfg.pref_path.c_str());
        ImGui::SetCursorPos(!is_default_path ? ImVec2((WINDOW_SIZE.x / 2.f) - BIG_BUTTON_SIZE.x - (20.f * SCALE.x), BIG_BUTTON_POS.y) : BIG_BUTTON_POS);
        if (ImGui::Button(lang["change_emu_path"].c_str(), BIG_BUTTON_SIZE)) {
            std::filesystem::path emulator_path = "";
            host::dialog::filesystem::Result result = host::dialog::filesystem::pick_folder(emulator_path);

            if ((result == host::dialog::filesystem::Result::SUCCESS) && (emulator_path.native() != emuenv.pref_path.native())) {
                emuenv.pref_path = fs::path(emulator_path.native()) / "";
                emuenv.cfg.set_pref_path(emuenv.pref_path);
            } else if (result == host::dialog::filesystem::Result::ERROR) {
                LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
            }
        }
        if (!is_default_path) {
            ImGui::SameLine(0, 40.f * SCALE.x);
            if (ImGui::Button(lang["reset_emu_path"].c_str(), BIG_BUTTON_SIZE)) {
                if (emuenv.default_path != emuenv.pref_path) {
                    emuenv.pref_path = emuenv.default_path;
                    emuenv.cfg.set_pref_path(emuenv.default_path);
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
            ImGui::SetWindowFontScale(RES_SCALE.x);
            draw_firmware_install_dialog(gui, emuenv);
        }
        break;
    case SELECT_INTERFACE_SETTINGS:
        title_str = lang["select_interface_settings"];
        ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize());
        ImGui::Checkbox(gui.lang.settings_dialog.gui["display_info_message"].c_str(), &emuenv.cfg.display_info_message);
        SetTooltipEx(gui.lang.settings_dialog.gui["display_info_message_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(gui.lang.settings_dialog.gui["display_system_apps"].c_str(), &emuenv.cfg.display_system_apps);
        SetTooltipEx(gui.lang.settings_dialog.gui["display_system_apps_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang["show_info_bar"].c_str(), &emuenv.cfg.show_info_bar);
        SetTooltipEx(lang["info_bar_description"].c_str());
        ImGui::SameLine();
        ImGui::Checkbox(lang["show_live_area_screen"].c_str(), &emuenv.cfg.show_live_area_screen);
        SetTooltipEx(lang["live_area_screen_description"].c_str());
        ImGui::Spacing();
        ImGui::Checkbox(lang["apps_list_grid"].c_str(), &emuenv.cfg.apps_list_grid);
        SetTooltipEx(lang["apps_list_grid_description"].c_str());
        if (!emuenv.cfg.apps_list_grid) {
            ImGui::Spacing();
            ImGui::SliderInt(lang["icon_size"].c_str(), &emuenv.cfg.icon_size, 64, 128);
            SetTooltipEx(lang["select_icon_size"].c_str());
        }
        break;
    case FINISHED:
        title_str = lang["completed"];
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(completed_setup).x / 2.f), (WINDOW_SIZE.y / 2.f) - ImGui::GetFontSize()));
        ImGui::Text("%s", completed_setup);
        ImGui::SetCursorPos(BIG_BUTTON_POS);
        if (ImGui::Button(common["ok"].c_str(), BIG_BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
            emuenv.cfg.initial_setup = true;
            config::serialize_config(emuenv.cfg, emuenv.base_path);
        }
        break;
    default: break;
    }

    ImGui::SetWindowFontScale(1.f);
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);

    // Draw Button
    ImGui::SetCursorPos(BUTTON_POS);
    if ((setup > SELECT_LANGUAGE) && ImGui::Button(lang["back"].c_str(), BUTTON_SIZE) || (setup > SELECT_LANGUAGE) && ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle)))
        setup = (InitialSetup)(setup - 1);
    ImGui::SetCursorPos(ImVec2(display_size.x - BUTTON_SIZE.x - BUTTON_POS.x, BUTTON_POS.y));
    if ((setup < FINISHED) && ImGui::Button(lang["next"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
        setup = (InitialSetup)(setup + 1);
        config::serialize_config(emuenv.cfg, emuenv.base_path);
    }
    ImGui::SetWindowFontScale(1.f);

    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopFont();
}

} // namespace gui
