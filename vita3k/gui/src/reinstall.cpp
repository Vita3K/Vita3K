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

#include <dialog/state.h>
#include <gui/functions.h>

#include <imgui.h>
#include <packages/sfo.h>

namespace gui {

void draw_reinstall_dialog(GenericDialogState *status, GuiState &gui, EmuEnvState &emuenv) {
    auto &lang = gui.lang.install_dialog.reinstall;
    auto &info = gui.lang.app_context.info;
    auto &common = emuenv.common_dialog.lang.common;

    ImGui::PushFont(gui.vita_font[emuenv.current_font_level]);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin(lang["reinstall_content"].c_str());
    ImGui::Text("%s", lang["already_installed"].c_str());
    ImGui::Spacing();
    ImGui::Text("%s: %s\n%s: %s\n%s: %s", info["name"].c_str(), emuenv.app_info.app_title.c_str(), info["title_id"].c_str(), emuenv.app_info.app_title_id.c_str(), info["version"].c_str(), emuenv.app_info.app_version.c_str());
    ImGui::Spacing();
    ImGui::Text("%s", lang["reinstall_overwrite"].c_str());
    if (ImGui::Button(common["yes"].c_str())) {
        *status = CONFIRM_STATE;
    }
    ImGui::SameLine();
    if (ImGui::Button(common["no"].c_str())) {
        *status = CANCEL_STATE;
    }
    ImGui::End();
    ImGui::PopFont();
}

} // namespace gui
