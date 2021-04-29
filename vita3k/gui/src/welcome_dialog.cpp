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

#include <sstream>

namespace gui {

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

void draw_welcome_dialog(GuiState &gui, HostState &host) {
    std::ostringstream link;
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
    ImGui::TextColored(GUI_COLOR_TEXT, "\nThe emulator is still in its early stages so any feedback and testing is greatly appreciated.");
    ImGui::TextColored(GUI_COLOR_TEXT, "\nTo get started, please install the Vita firmware and font packages.");
    ImGui::TextColored(GUI_COLOR_TEXT, "\nA comprehensive guide on how to set-up Vita3K can be found on the");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 9.f);
    if (ImGui::Button("Quickstart")) {
        link << OS_PREFIX << "https://vita3k.org/quickstart.html";
        system(link.str().c_str());
    }
    ImGui::SameLine();
    ImGui::TextColored(GUI_COLOR_TEXT, "page.");
    ImGui::TextColored(GUI_COLOR_TEXT, "Consult the Commercial game");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.f);
    if (ImGui::Button("compatibility")) {
        link << OS_PREFIX << "https://vita3k.org/compatibility.html";
        system(link.str().c_str());
    }
    ImGui::SameLine();
    ImGui::TextColored(GUI_COLOR_TEXT, "list and the Homebrew compatibility list to see what runs.");
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT, "Contributions are welcome!");
    ImGui::TextColored(GUI_COLOR_TEXT, "\nAdditional support can be found in the #help channel of the");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 9.f);
    if (ImGui::Button("Discord")) {
        link << OS_PREFIX << "https://discord.gg/6aGwQzh";
        system(link.str().c_str());
    }
    ImGui::TextColored(GUI_COLOR_TEXT, "Support us on");
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.f);
    if (ImGui::Button("Patreon")) {
        link << OS_PREFIX << "https://patreon.com/vita3k";
        system(link.str().c_str());
    }
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
