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

#include <ctrl/state.h>

namespace gui {

void draw_controllers_dialog(GuiState &gui, EmuEnvState &emuenv) {
    auto &lang = gui.lang.controllers;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin(lang["title"].c_str(), &gui.controls_menu.controllers_dialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    auto &ctrl = emuenv.ctrl;
    if (ctrl.controllers_num) {
        const auto connected_str = fmt::format(fmt::runtime(lang["connected"].c_str()), ctrl.controllers_num);
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", connected_str.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::BeginTable("main", 2)) {
            ImGui::TableSetupColumn("num");
            ImGui::TableSetupColumn("name");
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["num"].c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["name"].c_str());
            ImGui::Spacing();
            for (auto i = 0; i < ctrl.controllers_num; i++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%-8d", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", ctrl.controllers_name[i]);
            }
            ImGui::EndTable();
        }
    } else
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", lang["not_connected"].c_str());

    if (emuenv.ctrl.has_motion_support) {
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Gamepad has motion support");
    }
    ImGui::End();
}

} // namespace gui
