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

#include <config/functions.h>

#include <gui/functions.h>

#include "private.h"

namespace gui {

void draw_welcome_dialog(GuiState &gui, HostState &host) {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    ImGui::Begin("Welcome to Vita3K", &gui.help_menu.welcome_dialog, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "Vita3K PlayStation Vita Emulator");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "Vita3K is an open-source PlayStation Vita emulator written in C++ for Windows, Linux and MacOS.");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "The emulator is still in its early stages so any feedback and testing is greatly appreciated.");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "To get started, please install the Vita firmware and font packages.");
    if (ImGui::Button("Download Firmware"))
        open_path("https://www.playstation.com/en-us/support/hardware/psvita/system-software/");
    ImGui::SameLine();
    if (ImGui::Button("Download firmware font package"))
        open_path("https://bit.ly/2P2rb0r");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "A comprehensive guide on how to set-up Vita3K can be found on the");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.f);
    if (ImGui::Button("Quickstart"))
        open_path("https://vita3k.org/quickstart.html");
    ImGui::SameLine();
    ImGui::TextColored(GUI_COLOR_TEXT, "page.");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "Consult the Commercial game");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.f);
    if (ImGui::Button("compatibility"))
        open_path("https://vita3k.org/compatibility.html");
    ImGui::SameLine();
    ImGui::TextColored(GUI_COLOR_TEXT, "list and the Homebrew compatibility list to see what runs.");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "Contributions are welcome!");
    if (ImGui::Button("GitHub"))
        open_path("https://github.com/Vita3K/Vita3K");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "Additional support can be found in the #help channel of the");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.f);
    if (ImGui::Button("Discord"))
        open_path("https://discord.gg/6aGwQzh");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "Vita3K does not condone piracy. You must dump your own games.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Show next time", &host.cfg.show_welcome))
        config::serialize_config(host.cfg, host.cfg.config_path);
    ImGui::Spacing();
    if (ImGui::Button("Close"))
        gui.help_menu.welcome_dialog = false;
    ImGui::End();
}

} // namespace gui
