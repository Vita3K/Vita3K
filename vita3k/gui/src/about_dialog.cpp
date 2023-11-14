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
    "0nza1101", "0x8080", "AtticFinder65536", "BogdanTheGeek", "bsinky",
    "bythos14", "cobalt2727", "CoffeeBrewer64", "Cpasjuste", "CreepNT",
    "Croden1999", "d3m3vilurr", "Danik2343", "DerRM", "devnoname120",
    "dima-xd", "dracc", "edwinr", "Felipefpl", "FlotterCodername",
    "Frain-Breeze", "francois-berder", "FromAlaska", "Ghabry", "hobyst",
    "HuanJiCanShang", "ichisadashioko", "illusion0001", "isJuhn",
    "jdoe0000000", "jlachniet", "Johnnynator", "johnothwolo", "Kaitul",
    "kd-11", "KhoraLee", "lephilousophe", "lybxlpsv", "MaddTheSane",
    "Margen67", "merryhime", "mirusu400", "mkersey", "mrcmunir",
    "muemart", "NabsiYa", "NeveHanter", "ngeojiajun", "Nicba1010",
    "nishinji", "nn9dev", "Princess-of-Sleeping", "qurious-pixel", "Ristellise",
    "scribam", "sitiom", "smitdylan2001", "SonicMastr", "Talkashie",
    "Tarik366", "tcoyvwac", "thp", "TingPing", "TomasKralCZ",
    "totlmstr", "wfscans", "xero-lib", "xerpi", "xyzz",
    "yousifd", "Yunotchi"
};

static std::vector<const char *> supporters_list = {
    "j0hnnybrav0", "TacoOblivion", "Undeadbob"
};

void draw_about_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const ImVec2 RES_SCALE(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);

    auto &lang = gui.lang.about;
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
    ImGui::Separator();
    ImGui::Spacing();

    const auto open_link = [](const char *name, const char *link) {
        ImGui::PushStyleColor(0, IM_COL32(0.f, 128.f, 255.f, 255.f));
        if (ImGui::Selectable(name, false, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcTextSize(name))))
            open_path(link);
        ImGui::PopStyleColor();
    };

    ImGui::Text("%s ", lang["special_credit"].c_str());
    ImGui::SameLine(0, 0);
    open_link("Gordon Mackay", "https://gordonmackayillustration.blogspot.com");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("%s ", lang["github_website"].c_str());
    ImGui::SameLine(0, 0);
    open_link("GitHub", "https://github.com/Vita3K/Vita3K");
    ImGui::Spacing();

    ImGui::Text("%s ", lang["vita3k_website"].c_str());
    ImGui::SameLine(0, 0);
    open_link("vita3k.org", "https://vita3k.org");
    ImGui::Spacing();

    ImGui::Text("%s ", lang["ko-fi_website"].c_str());
    ImGui::SameLine(0, 0);
    open_link("Ko-fi", "https://ko-fi.com/vita3k");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Draw Vita3K Staff list
    ImGui::SetCursorPosX(HALF_WINDOW_WIDTH - (ImGui::CalcTextSize(lang["vita3k_staff"].c_str()).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%s", lang["vita3k_staff"].c_str());
    ImGui::Spacing();

    const auto STAFF_LIST_SIZE = ImVec2(630.f * SCALE.x, 160.f * SCALE.y);
    static constexpr int STAFF_COLUMN_COUNT(3);
    const float STAFF_COLUMN_SIZE(STAFF_LIST_SIZE.x / STAFF_COLUMN_COUNT);
    const float STAFF_COLUMN_POS(HALF_WINDOW_WIDTH - (STAFF_LIST_SIZE.x / 2.f));

    ImGui::SetCursorPosX(STAFF_COLUMN_POS);
    if (ImGui::BeginTable("##vita3k_staff_table", STAFF_COLUMN_COUNT, ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoSavedSettings, STAFF_LIST_SIZE)) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        ImGui::TableSetupColumn(lang["developers"].c_str(), ImGuiTableColumnFlags_NoHide, STAFF_COLUMN_SIZE);
        ImGui::TableSetupColumn(lang["contributors"].c_str(), ImGuiTableColumnFlags_NoHide, STAFF_COLUMN_SIZE);
        ImGui::TableSetupColumn(lang["supporters"].c_str(), ImGuiTableColumnFlags_NoHide, STAFF_COLUMN_SIZE);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        const auto open_author_history = [&](const char *author) {
            open_link(author, fmt::format("https://github.com/Vita3K/Vita3K/commits?author={}", author).c_str());
        };

        // Developers list
        for (const auto developer : developers_list)
            open_author_history(developer);
        ImGui::TableNextColumn();

        // Contributors list
        for (const auto contributor : contributors_list)
            open_author_history(contributor);
        ImGui::TableNextColumn();

        // Supporters list
        for (const auto supporter : supporters_list)
            ImGui::Text("%s", supporter);
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace gui
