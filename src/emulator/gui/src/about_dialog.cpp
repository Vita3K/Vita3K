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

#include <host/state.h>
#include <host/version.h>

#include <sstream>

namespace gui {

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

void draw_about_dialog(HostState &host) {
    float width = ImGui::GetWindowWidth() / 0.65;
    float height = ImGui::GetWindowHeight() / 1.35;

    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("About", &host.gui.help_menu.about_dialog);

    ImGui::Text("%s", window_title);

    ImGui::Separator();

    ImGui::Text("\nVita3K: a PS Vita/PSTV Emulator. The world's first functional PSVita/PSTV emulator");
    ImGui::Text("Vita3K is an experimental open-source PlayStation Vita/PlayStation TV emulator\nwritten in C++ for Windows, MacOS and Linux operating systems.");
    ImGui::Text("\nNote: The emulator is still in a very early stage of development.");

    ImGui::Separator();

    ImGui::Text("If you'd like to show your support to the project, you can donate to our Patreon:");

    std::ostringstream link;

    if (ImGui::Button("patreon.com/vita3k")) {
        std::string patreon_url = "https://patreon.com/vita3k";
        link << OS_PREFIX << patreon_url;
        system(link.str().c_str());
    }

    ImGui::Text("Or if you're interested in contributing, check out our Github:");

    if (ImGui::Button("github.com/vita3k/vita3k")) {
        std::string github_url = "https://github.com/vita3k/vita3k";
        link << OS_PREFIX << github_url;
        system(link.str().c_str());
    }

    ImGui::Text("Visit our website for more info:");

    if (ImGui::Button("vita3k.org")) {
        std::string website_url = "https://vita3k.org";
        link << OS_PREFIX << website_url;
        system(link.str().c_str());
    }

    ImGui::End();
}

} // namespace gui
