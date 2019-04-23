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
#include <util/string_utils.h>

using namespace std::string_literals;

namespace gui {

static constexpr auto MENUBAR_HEIGHT = 19;

void draw_game_selector(HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiSetCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT), ImGuiSetCond_Always);
    if (host.cfg.background_alpha)
        ImGui::SetNextWindowBgAlpha(host.cfg.background_alpha.value());
    ImGui::Begin("Game Selector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    static ImGuiTextFilter search_bar;
    search_bar.Draw("Search", 300);

    if (host.gui.background_texture) {
        ImGui::GetBackgroundDrawList()->AddImage(reinterpret_cast<void *>(host.gui.background_texture.get()),
            ImVec2(0, 0), display_size);
    }

    switch (host.gui.game_selector.state) {
    case SELECT_APP:
        ImGui::Columns(3);
        std::string title_id_label = "TitleID";
        switch (host.gui.game_selector.title_id_sort_state) {
        case ASCENDANT:
            title_id_label += " >";
            ImGui::SetColumnWidth(0, 100);
            break;
        case DESCENDANT:
            title_id_label += " <";
            ImGui::SetColumnWidth(0, 100);
            break;
        default:
            ImGui::SetColumnWidth(0, 80);
            break;
        }
        ImGui::SetWindowFontScale(1.1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        if (ImGui::Button(title_id_label.c_str())) {
            host.gui.game_selector.title_id_sort_state = static_cast<gui::SortState>(std::max(1, (host.gui.game_selector.title_id_sort_state + 1) % 3));
            host.gui.game_selector.app_ver_sort_state = NOT_SORTED;
            host.gui.game_selector.title_sort_state = NOT_SORTED;
            switch (host.gui.game_selector.title_id_sort_state) {
            case ASCENDANT:
                std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return lhs.title_id < rhs.title_id;
                });
                break;
            case DESCENDANT:
                std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return lhs.title_id > rhs.title_id;
                });
                break;
            default:
                break;
            }
        }
        ImGui::NextColumn();
        std::string app_ver_label = "Version";
        switch (host.gui.game_selector.app_ver_sort_state) {
        case ASCENDANT:
            app_ver_label += " >";
            ImGui::SetColumnWidth(1, 90);
            break;
        case DESCENDANT:
            app_ver_label += " <";
            ImGui::SetColumnWidth(1, 90);
            break;
        default:
            ImGui::SetColumnWidth(1, 70);
            break;
        }
        if (ImGui::Button(app_ver_label.c_str())) {
            host.gui.game_selector.title_id_sort_state = NOT_SORTED;
            host.gui.game_selector.app_ver_sort_state = static_cast<gui::SortState>(std::max(1, (host.gui.game_selector.app_ver_sort_state + 1) % 3));
            host.gui.game_selector.title_sort_state = NOT_SORTED;
            switch (host.gui.game_selector.app_ver_sort_state) {
            case ASCENDANT:
                std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return lhs.app_ver < rhs.app_ver;
                });
                break;
            case DESCENDANT:
                std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return lhs.app_ver > rhs.app_ver;
                });
                break;
            default:
                break;
            }
        }
        ImGui::NextColumn();
        std::string title_label = "Title";
        switch (host.gui.game_selector.title_sort_state) {
        case ASCENDANT:
            title_label += " >";
            break;
        case DESCENDANT:
            title_label += " <";
            break;
        default:
            break;
        }
        if (ImGui::Button(title_label.c_str()) || !host.gui.game_selector.is_game_list_sorted) {
            host.gui.game_selector.title_id_sort_state = NOT_SORTED;
            host.gui.game_selector.app_ver_sort_state = NOT_SORTED;
            host.gui.game_selector.title_sort_state = static_cast<gui::SortState>(std::max(1, (host.gui.game_selector.title_sort_state + 1) % 3));
            host.gui.game_selector.is_game_list_sorted = true;
            switch (host.gui.game_selector.title_sort_state) {
            case ASCENDANT:
                std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
                });
                break;
            case DESCENDANT:
                std::sort(host.gui.game_selector.games.begin(), host.gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return string_utils::toupper(lhs.title) > string_utils::toupper(rhs.title);
                });
                break;
            default:
                break;
            }
        }
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        for (auto game : host.gui.game_selector.games) {
            bool selected_1 = false;
            bool selected_2 = false;
            bool selected_3 = false;
            if (!search_bar.PassFilter(game.title.c_str()) && !search_bar.PassFilter(game.title_id.c_str()))
                continue;
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
