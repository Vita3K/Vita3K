// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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
#include <imgui.h>

#include <gui/gui_constants.h>
#include <host/state.h>

void DrawControlsDialog(HostState &host) {
    float width = ImGui::GetWindowWidth() / 1.35;
    float height = ImGui::GetWindowHeight() / 1.35;
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowPosCenter();
    ImGui::Begin("Controls", &host.gui.controls_dialog);

    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-16s", "Button", "Mapped button");
    ImGui::Text("%-16s    %-16s", "Left stick", "WASD");
    ImGui::Text("%-16s    %-16s", "Right stick", "IJKL");
    ImGui::Text("%-16s    %-16s", "D-pad", "Arrow keys");
    if (host.cfg.pstv_mode) {
        ImGui::Text("%-16s    %-16s", "L1 button", "Q");
        ImGui::Text("%-16s    %-16s", "R1 button", "E");
        ImGui::Text("%-16s    %-16s", "L2 button", "U");
        ImGui::Text("%-16s    %-16s", "R2 button", "O");
        ImGui::Text("%-16s    %-16s", "L3 button", "F");
        ImGui::Text("%-16s    %-16s", "R3 button", "H");
    } else {
        ImGui::Text("%-16s    %-16s", "L button", "Q");
        ImGui::Text("%-16s    %-16s", "R button", "E");
    }
    ImGui::Text("%-16s    %-16s", "Square button", "Z");
    ImGui::Text("%-16s    %-16s", "X button", "X");
    ImGui::Text("%-16s    %-16s", "Circle button", "C");
    ImGui::Text("%-16s    %-16s", "Triangle button", "V");
    ImGui::Text("%-16s    %-16s", "Start button", "Enter");
    ImGui::Text("%-16s    %-16s", "Select button", "Right shift");

    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s", "GUI");
    ImGui::Text("%-16s    %-16s", "Toggle GUI visibility", "G");

    ImGui::End();
}
