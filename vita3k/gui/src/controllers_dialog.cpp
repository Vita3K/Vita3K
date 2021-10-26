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

#include "private.h"

#include <ctrl/functions.h>

namespace gui {

void draw_controllers_dialog(GuiState &gui, HostState &host) {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("Controllers", &gui.controls_menu.controllers_dialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    auto &ctrl = host.ctrl;
    refresh_controllers(ctrl);
    if (ctrl.controllers_num) {
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%d Controllers connected", ctrl.controllers_num);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-8s Name", "Num");
        ImGui::Spacing();
        for (auto i = 0; i < ctrl.controllers_num; i++) {
            ImGui::Text("%-8d", i);
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(7.f, 7.f));
            ImGui::SameLine();
            ImGui::Text("%s", ctrl.controllers_name[i]);
        }
    } else
        ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "No compatible controllers connected.\nConnect a controller that is compatible with SDL2.");
    ImGui::End();
}

} // namespace gui
