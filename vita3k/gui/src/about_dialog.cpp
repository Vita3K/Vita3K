// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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
#include <dialog/state.h>

namespace gui {

// Add any new developer/contributor in alphabetic order
static constexpr std::array developers_list = {
    "1whatleytay", "bookmist", "EXtremeExploit", "frangarcj", "IllusionMan1212",
    "KorewaWatchful", "Macdu", "pent0", "petmac", "Rinnegatamante",
    "sunho", "VelocityRa", "Zangetsu38"
};

static constexpr std::array contributors_list = {
    "0nza1101", "0x8080", "AtticFinder65536", "Avellea", "blackbird806",
    "BogdanTheGeek", "bsinky", "bythos14", "cobalt2727", "CoffeeBrewer64", "Cpasjuste",
    "Creeot", "CreepNT", "Croden1999", "d3m3vilurr", "Danik2343", "darkash",
    "DerRM", "devnoname120", "dima-xd", "dracc", "edwinr", "FantasyGmm",
    "Felipefpl", "FlotterCodername", "Frain-Breeze", "francois-berder", "FromAlaska",
    "Ghabry", "hobyst", "HuanJiCanShang", "ichisadashioko", "illusion0001",
    "isJuhn", "jdoe0000000", "jlachniet", "Johnnynator", "johnothwolo", "Kaitul",
    "KaneDbD", "kd-11", "KhoraLee", "Kitakatarashima", "lephilousophe",
    "Lupiax", "lybxlpsv", "MaddTheSane", "Margen67", "mavethee", "merryhime",
    "mirusu400", "mkersey", "mrcmunir", "muemart", "NeveHanter", "ngeojiajun",
    "Nicba1010", "nishinji", "nn9dev", "Nyabsi", "oltolm", "pedrozzz0",
    "Princess-of-Sleeping", "qurious-pixel", "redpolline", "Ristellise",
    "saturnsky", "scribam", "sitiom", "slipcounter", "smitdylan2001",
    "SonicMastr", "Talkashie", "Tarik366", "tcoyvwac", "thp", "TingPing",
    "TomasKralCZ", "totlmstr", "Valeriy-Lednikov", "wfscans", "xero-lib",
    "xerpi", "xperia64", "xsamueljr", "xyzz", "yousifd", "Yunotchi"
};

static constexpr std::array supporters_list = {
    "j0hnnybrav0", "TacoOblivion", "Undeadbob", "uplush"
};

void draw_about_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    static const auto BUTTON_SIZE = ImVec2(120.f * emuenv.manual_dpi_scale, 0.f);

    auto &lang = gui.lang.about;
    auto &common = emuenv.common_dialog.lang.common;

    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##about", &gui.help_menu.about_dialog, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang["title"].c_str());
    ImGui::Spacing();
    ImGui::Separator();
    TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, window_title);

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
    TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, lang["vita3k_staff"].c_str());
    ImGui::Spacing();

    const auto STAFF_LIST_SIZE = ImVec2(630.f * SCALE.x, 160.f * SCALE.y);
    static constexpr int STAFF_COLUMN_COUNT(3);
    const float STAFF_COLUMN_SIZE(STAFF_LIST_SIZE.x / STAFF_COLUMN_COUNT);
    const float STAFF_COLUMN_POS(ImGui::GetWindowWidth() / 2.f - (STAFF_LIST_SIZE.x / 2.f));

    ImGui::SetCursorPosX(STAFF_COLUMN_POS);
    if (ImGui::BeginTable("##vita3k_staff_table", STAFF_COLUMN_COUNT, ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoSavedSettings, STAFF_LIST_SIZE)) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        ImGui::TableSetupColumn(lang["developers"].c_str(), ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed, STAFF_COLUMN_SIZE);
        ImGui::TableSetupColumn(lang["contributors"].c_str(), ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed, STAFF_COLUMN_SIZE);
        ImGui::TableSetupColumn(lang["supporters"].c_str(), ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed, STAFF_COLUMN_SIZE);
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
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (BUTTON_SIZE.x / 2.f));
    if (ImGui::Button(common["close"].c_str(), BUTTON_SIZE))
        gui.help_menu.about_dialog = false;

    ImGui::End();
}

} // namespace gui
