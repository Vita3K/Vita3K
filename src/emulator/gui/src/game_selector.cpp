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

using namespace std::string_literals;

namespace gui {

static constexpr auto MENUBAR_HEIGHT = 19;

void draw_game_selector(HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiSetCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT), ImGuiSetCond_Always);
    ImGui::Begin("Game Selector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    switch (host.gui.game_selector.state) {
    case SELECT_APP:
        ImGui::Columns(3);
        ImGui::SetColumnWidth(0, 80);
        ImGui::SetWindowFontScale(1.1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        if (ImGui::Button("TitleID")) {
            std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                return lhs.title_id < rhs.title_id;
            });
            host.gui.game_selector.is_game_list_sorted = true;
        }
        ImGui::NextColumn();
        ImGui::SetColumnWidth(1, 70);
        if (ImGui::Button("Version")) {
            std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                return lhs.app_ver < rhs.app_ver;
            });
            host.gui.game_selector.is_game_list_sorted = true;
        }
        ImGui::NextColumn();
        if (ImGui::Button("Title") || !host.gui.game_selector.is_game_list_sorted) {
            std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                return lhs.title < rhs.title;
            });
            host.gui.game_selector.is_game_list_sorted = true;
        }
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        for (auto game : host.gui.game_selector.games) {
            bool selected_1 = false;
            bool selected_2 = false;
            bool selected_3 = false;
            ImGui::Selectable(game.title_id.c_str(), &selected_1, ImGuiSelectableFlags_SpanAllColumns);
            ImGui::NextColumn();
            ImGui::Selectable(game.app_ver.c_str(), &selected_2, ImGuiSelectableFlags_SpanAllColumns);
            ImGui::NextColumn();
            ImGui::Selectable(game.title.c_str(), &selected_3, ImGuiSelectableFlags_SpanAllColumns);
            ImGui::NextColumn();
            if (selected_1 || selected_2 || selected_3) {
                host.gui.game_selector.selected_title_id = game.title_id;
            }
        }
        ImGui::PopStyleColor(2);
        break;
    }
    ImGui::End();
}

} // namespace gui
