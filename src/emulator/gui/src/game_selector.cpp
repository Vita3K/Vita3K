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

    if (host.gui.background_texture) {
        ImGui::GetBackgroundDrawList()->AddImage(reinterpret_cast<void *>(host.gui.background_texture.get()),
            ImVec2(0, 0), display_size);
    }

    constexpr float default_icon_scale = 64.0f;
    float icon_scale = default_icon_scale;

    const auto icon_size = static_cast<gui::IconSize>(host.cfg.icon_size);
    switch (icon_size) {
    case ICON_SIZE_SMALL: icon_scale = icon_scale / 2; break;
    case ICON_SIZE_DEFAULT: break;
    case ICON_SIZE_MEDIUM: icon_scale = icon_scale + 16; break;
    case ICON_SIZE_LARGE: icon_scale = icon_scale + 32; break;
    case ICON_SIZE_ORIGINAL: icon_scale = icon_scale * 2; break;
    default: break;
    }

    switch (host.gui.game_selector.state) {
    case SELECT_APP:
        ImGui::Columns(4);
        ImGui::SetWindowFontScale(1.1f);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        ImGui::Button("Icon"); // Button to fit style, does nothing.
        ImGui::SetColumnWidth(0, icon_scale + /* padding */ 20);
        ImGui::NextColumn();
        std::string title_id_label = "TitleID";
        switch (host.gui.game_selector.title_id_sort_state) {
        case ASCENDANT:
            title_id_label += " >";
            ImGui::SetColumnWidth(1, 100);
            break;
        case DESCENDANT:
            title_id_label += " <";
            ImGui::SetColumnWidth(1, 100);
            break;
        default:
            ImGui::SetColumnWidth(1, 80);
            break;
        }
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
            ImGui::SetColumnWidth(2, 90);
            break;
        case DESCENDANT:
            app_ver_label += " <";
            ImGui::SetColumnWidth(2, 90);
            break;
        default:
            ImGui::SetColumnWidth(2, 70);
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
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_SEARCH_BAR_TEXT);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_COLOR_SEARCH_BAR_BG);
        ImGui::SameLine(ImGui::GetColumnWidth() - (ImGui::CalcTextSize("Search").x + ImGui::GetStyle().DisplayWindowPadding.x + 300));
        ImGui::TextColored(GUI_COLOR_TEXT, "Search");
        ImGui::SameLine();
        search_bar.Draw("", 300);

        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        for (const auto &game : host.gui.game_selector.games) {
            bool selected[4] = { false, false, false, false };
            if (!search_bar.PassFilter(game.title.c_str()) && !search_bar.PassFilter(game.title_id.c_str()))
                continue;
            if (host.gui.game_selector.icons[game.title_id]) {
                GLuint texture = host.gui.game_selector.icons[game.title_id].get();
                if (ImGui::ImageButton(reinterpret_cast<void *>(texture), ImVec2(icon_scale, icon_scale))) {
                    selected[0] = true;
                }
            }
            ImGui::NextColumn();
            ImGui::Selectable(game.title_id.c_str(), &selected[1], ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, icon_scale));
            ImGui::NextColumn();
            ImGui::Selectable(game.app_ver.c_str(), &selected[2], ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, icon_scale));
            ImGui::NextColumn();
            ImGui::Selectable(game.title.c_str(), &selected[3], ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, icon_scale));
            ImGui::NextColumn();
            if (std::find(std::begin(selected), std::end(selected), true) != std::end(selected)) {
                host.gui.game_selector.selected_title_id = game.title_id;
            }
        }
        ImGui::PopStyleColor(4);
        break;
    }
    ImGui::End();
}

} // namespace gui
