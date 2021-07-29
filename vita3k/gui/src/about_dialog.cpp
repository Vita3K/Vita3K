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

#include "private.h"

#include <config/version.h>

namespace gui {

void draw_about_dialog(GuiState &gui, HostState &host) {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("About", &gui.help_menu.about_dialog, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("%s", window_title);

    ImGui::Separator();

    ImGui::Text("\nVita3K: a PS Vita/PSTV Emulator. The world's first functional PSVita/PSTV emulator");
    ImGui::Text("Vita3K is an experimental open-source PlayStation Vita/PlayStation TV emulator\nwritten in C++ for Windows, MacOS and Linux operating systems.");
    ImGui::Text("\nNote: The emulator is still in a very early stage of development.");

    ImGui::Separator();

    ImGui::Text("If you'd like to show your support to the project, you can donate to our Patreon:");

    if (ImGui::Button("Patreon"))
        open_path("https://patreon.com/vita3k");

    ImGui::Text("Or if you're interested in contributing, check out our Github:");

    if (ImGui::Button("Github"))
        open_path("https://github.com/vita3k/vita3k");

    ImGui::Text("Visit our website for more info:");

    if (ImGui::Button("vita3k.org"))
        open_path("https://vita3k.org");

    ImGui::End();
}

} // namespace gui
