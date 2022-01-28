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

#include <gui/functions.h>

#include "private.h"

#include <config/version.h>

namespace gui {

// Add any new developer/contribor in alphabetic order
static std::vector<const char *> developers_list = {
    "1whatleytay", "frangarcj", "IllusionMan1212", "KorewaWatchful", "pent0",
    "petmac", "Rinnegatamante", "sunho", "VelocityRa", "Zangetsu38 "
};

static std::vector<const char *> contributors_list = {
    "0nza1101", "0x8080", "BogdanTheGeek", "bookmist", "bsinky",
    "bythos14", "Cpasjuste", "CreepNT", "d3m3vilurr", "DerRM",
    "devnoname120", "dracc", "EXtremeExploit", "Felipefpl", "Frain-Breeze",
    "francois-berder", "FromAlaska", "Ghabry", "hobyst", "ichisadashioko",
    "isJuhn", "jlachniet", "Johnnynator", "johnothwolo", "kd-11",
    "korenkonder", "MaddTheSane", "Margen67", "merryhime", "mkersey",
    "mrcmunir", "muemart", "NeveHanter", "Nicba1010", "Princess-of-Sleeping",
    "qurious-pixel", "Ristellise", "scribam", "smitdylan2001", "strikersh",
    "Talkashie", "TarikYildiz366", "TehPsychedelic", "thp", "tcoyvwac",
    "TingPing", "TomasKralCZ", "totlmstr", "wfscans", "xyzz",
    "yousifd", "Yunotchi"
};

void draw_about_dialog(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("About", &gui.help_menu.about_dialog, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    const auto HALF_WINDOW_WIDTH = ImGui::GetWindowWidth() / 2.f;
    ImGui::SetCursorPosX(HALF_WINDOW_WIDTH - (ImGui::CalcTextSize(window_title).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", window_title);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Vita3K: a PS Vita/PSTV Emulator. The world's first functional PSVita/PSTV emulator");
    ImGui::Spacing();
    ImGui::TextWrapped("Vita3K is an experimental open-source PlayStation Vita/PlayStation TV emulator written in C++ for Windows, MacOS and Linux operating systems.");
    ImGui::Spacing();
    ImGui::Text("Note: The emulator is still in a very early stage of development.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("If you're interested in contributing, check out our Github:");
    if (ImGui::Button("Github"))
        open_path("https://github.com/vita3k/vita3k");
    ImGui::Spacing();

    ImGui::Text("Visit our website for more info:");
    if (ImGui::Button("vita3k.org"))
        open_path("https://vita3k.org");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Draw Vita3K Staff list
    ImGui::SetCursorPosX(HALF_WINDOW_WIDTH - (ImGui::CalcTextSize("Vita3K Staff").x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "Vita3K Staff");
    ImGui::Spacing();

    const auto STAFF_LIST_SIZE = ImVec2(360.f * host.dpi_scale, 160.f * host.dpi_scale);
    const auto HALF_STAFF_LIST_WIDTH = STAFF_LIST_SIZE.x / 2.f;

    ImGui::SetCursorPosX(HALF_WINDOW_WIDTH - HALF_STAFF_LIST_WIDTH);
    ImGui::BeginChild("##vita3k_staff", STAFF_LIST_SIZE, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Columns(2, nullptr, true);
    ImGui::SetColumnWidth(0, HALF_STAFF_LIST_WIDTH);
    ImGui::SetColumnWidth(1, HALF_STAFF_LIST_WIDTH);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Developers");
    ImGui::NextColumn();
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Contributors");
    ImGui::NextColumn();
    ImGui::Separator();
    for (const auto developer : developers_list)
        ImGui::Text("%s", developer);
    ImGui::NextColumn();
    for (const auto contributor : contributors_list)
        ImGui::Text("%s", contributor);
    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::EndChild();

    ImGui::End();
}

} // namespace gui
