// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <dialog/state.h>
#include <gui/functions.h>

#include <packages/sfo.h>

namespace gui {

void draw_reinstall_dialog(GenericDialogState *status, GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const auto BUTTON_SIZE = ImVec2(180.f * SCALE.x, 45.f * SCALE.y);
    const auto WINDOW_SIZE = ImVec2(616.f * SCALE.x, 264.f * SCALE.y);

    auto &lang = gui.lang.install_dialog.reinstall;
    auto &info = gui.lang.app_context.info;
    auto &common = emuenv.common_dialog.lang.common;

    ImGui::PushFont(gui.vita_font[emuenv.current_font_level]);
    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size);
    ImGui::Begin("##reinstall", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2.f), emuenv.logical_viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##reinstall_child", WINDOW_SIZE, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const auto POS_BUTTON = (ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang["reinstall_content"].c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["already_installed"].c_str());
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s: %s\n%s: %s\n%s: %s", info["name"].c_str(), emuenv.app_info.app_title.c_str(), info["title_id"].c_str(), emuenv.app_info.app_title_id.c_str(), info["version"].c_str(), emuenv.app_info.app_version.c_str());
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["reinstall_overwrite"].c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - BUTTON_SIZE.x - (10.f * SCALE.x));
    if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE))
        *status = CANCEL_STATE;
    ImGui::SameLine(0, 20.f * SCALE.x);
    if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE))
        *status = CONFIRM_STATE;
    ImGui::SameLine(0, 20.f * SCALE.x);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopFont();
}

} // namespace gui
