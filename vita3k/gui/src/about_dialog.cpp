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

#include <gui/functions.h>

#include "private.h"

#include <config/version.h>

namespace gui {

// Add any new developer/contribor in alphabetic order
static std::vector<const char *> developers_list = {
    "1whatleytay", "bookmist", "EXtremeExploit", "frangarcj", "IllusionMan1212",
    "KorewaWatchful", "Macdu", "pent0", "petmac", "Rinnegatamante",
    "sunho", "VelocityRa", "Zangetsu38"
};

static std::vector<const char *> contributors_list = {
    "0nza1101", "0x8080", "ArbestRi", "BogdanTheGeek", "bsinky",
    "bythos14", "Cpasjuste", "CreepNT", "Croden1999", "d3m3vilurr",
    "DerRM", "devnoname120", "dracc", "Felipefpl", "Frain-Breeze",
    "francois-berder", "FromAlaska", "Ghabry", "hobyst", "ichisadashioko",
    "isJuhn", "jlachniet", "Johnnynator", "johnothwolo", "kd-11",
    "korenkonder", "MaddTheSane", "Margen67", "merryhime", "mkersey",
    "mrcmunir", "muemart", "NeveHanter", "Nicba1010", "Princess-of-Sleeping",
    "qurious-pixel", "Ristellise", "scribam", "smitdylan2001", "strikersh",
    "Talkashie", "TarikYildiz366", "TehPsychedelic", "thp", "tcoyvwac",
    "TingPing", "TomasKralCZ", "totlmstr", "wfscans", "xerpi",
    "xyzz", "yousifd", "Yunotchi"
};

void draw_about_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
  
    auto lang = gui.lang.about;
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin(lang["title"].c_str(), &gui.help_menu.about_dialog, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    const auto HALF_WINDOW_WIDTH = ImGui::GetWindowWidth() / 2.f;
    ImGui::SetCursorPosX(HALF_WINDOW_WIDTH - (ImGui::CalcTextSize(window_title).x / 2.f));
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", window_title);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("%s", lang["vita3k"].c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("%s", lang["about_vita3k"].c_str());
    ImGui::Spacing();
    ImGui::Text("%s", lang["note"].c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("%s", lang["github_website"].c_str());
    if (ImGui::Button("GitHub"))
        open_path("https://github.com/vita3k/vita3k");
    ImGui::Spacing();

    ImGui::Text("%s", lang["vita3k_website"].c_str());
    if (ImGui::Button("vita3k.org"))
        open_path("https://vita3k.org");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Draw Vita3K Staff list
    ImGui::SetCursorPosX(HALF_WINDOW_WIDTH - (ImGui::CalcTextSize(lang["vita3k_staff"].c_str()).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", lang["vita3k_staff"].c_str());
    ImGui::Spacing();

    const auto STAFF_LIST_SIZE = ImVec2(440.f * emuenv.dpi_scale, 160.f * emuenv.dpi_scale);
    const auto HALF_STAFF_LIST_WIDTH = STAFF_LIST_SIZE.x / 2.f;

    ImGui::SetCursorPosX(HALF_WINDOW_WIDTH - HALF_STAFF_LIST_WIDTH);
    ImGui::BeginChild("##vita3k_staff", STAFF_LIST_SIZE, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Columns(2, nullptr, true);
    ImGui::SetColumnWidth(0, HALF_STAFF_LIST_WIDTH);
    ImGui::SetColumnWidth(1, HALF_STAFF_LIST_WIDTH);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["developers"].c_str());
    ImGui::NextColumn();
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["contributors"].c_str());
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
