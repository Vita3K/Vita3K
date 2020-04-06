// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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

#include "private.h"

#include <gui/functions.h>
#include <gui/state.h>

#include <host/state.h>

#include <util/log.h>
#include <util/string_utils.h>

namespace gui {

bool refresh_game_list(GuiState &gui, HostState &host) {
    auto game_list_size = gui.game_selector.games.size();

    if (gui.current_background != gui.user_backgrounds[host.cfg.background_image])
        gui.current_background = gui.user_backgrounds[host.cfg.background_image];
    gui.game_selector.games.clear();
    gui.game_selector.icons.clear();
    gui.live_area_contents.clear();
    gui.live_items.clear();
    gui.delete_game_background = true;

    get_game_titles(gui, host);

    if (gui.game_selector.games.empty())
        return false;

    init_icons(gui, host);

    std::sort(gui.game_selector.games.begin(), gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
        return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
    });

    std::string change_game_list = "new game(s) added";
    if (game_list_size == gui.game_selector.games.size())
        return false;
    else if (game_list_size > gui.game_selector.games.size()) {
        change_game_list = "game(s) removed";
        game_list_size -= gui.game_selector.games.size();
    } else
        game_list_size = gui.game_selector.games.size() - game_list_size;

    LOG_INFO("{} {}", game_list_size, change_game_list);

    return true;
}

static constexpr auto MENUBAR_HEIGHT = 19;

void draw_game_selector(GuiState &gui, HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT), ImGuiCond_Always);
    if (gui.current_background)
        ImGui::SetNextWindowBgAlpha(host.cfg.background_alpha);
    ImGui::Begin("Game Selector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    if (gui.current_background) {
        ImGui::GetBackgroundDrawList()->AddImage(reinterpret_cast<void *>(gui.current_background),
            ImVec2(0, 0), display_size);
    }

    if (gui.delete_game_background) {
        gui.game_backgrounds.clear();
        gui.delete_game_background = false;
    }

    if (gui.delete_game_icon) {
        if (gui.game_selector.icons.find(host.io.title_id) != gui.game_selector.icons.end())
            gui.game_selector.icons.erase(host.io.title_id);
        gui.delete_game_icon = false;
    }

    const float icon_size = static_cast<float>(host.cfg.icon_size);

    switch (gui.game_selector.state) {
    case SELECT_APP:
        ImGui::Columns(4);
        ImGui::SetWindowFontScale(1.1f);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        ImGui::Button("Icon"); // Button to fit style, does nothing.
        ImGui::SetColumnWidth(0, icon_size + /* padding */ 20);
        ImGui::NextColumn();
        std::string title_id_label = "TitleID";
        switch (gui.game_selector.title_id_sort_state) {
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
            gui.game_selector.title_id_sort_state = static_cast<gui::SortState>(std::max<int>(1, (gui.game_selector.title_id_sort_state + 1) % 3));
            gui.game_selector.app_ver_sort_state = NOT_SORTED;
            gui.game_selector.title_sort_state = NOT_SORTED;
            switch (gui.game_selector.title_id_sort_state) {
            case ASCENDANT:
                std::sort(gui.game_selector.games.begin(), gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return lhs.title_id < rhs.title_id;
                });
                break;
            case DESCENDANT:
                std::sort(gui.game_selector.games.begin(), gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return lhs.title_id > rhs.title_id;
                });
                break;
            default:
                break;
            }
        }
        ImGui::NextColumn();
        std::string app_ver_label = "Version";
        switch (gui.game_selector.app_ver_sort_state) {
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
            gui.game_selector.title_id_sort_state = NOT_SORTED;
            gui.game_selector.app_ver_sort_state = static_cast<gui::SortState>(std::max<int>(1, (gui.game_selector.app_ver_sort_state + 1) % 3));
            gui.game_selector.title_sort_state = NOT_SORTED;
            switch (gui.game_selector.app_ver_sort_state) {
            case ASCENDANT:
                std::sort(gui.game_selector.games.begin(), gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return lhs.app_ver < rhs.app_ver;
                });
                break;
            case DESCENDANT:
                std::sort(gui.game_selector.games.begin(), gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return lhs.app_ver > rhs.app_ver;
                });
                break;
            default:
                break;
            }
        }
        ImGui::NextColumn();
        std::string title_label = "Title";
        switch (gui.game_selector.title_sort_state) {
        case ASCENDANT:
            title_label += " >";
            break;
        case DESCENDANT:
            title_label += " <";
            break;
        default:
            break;
        }
        if (ImGui::Button(title_label.c_str()) || !gui.game_selector.is_game_list_sorted) {
            gui.game_selector.title_id_sort_state = NOT_SORTED;
            gui.game_selector.app_ver_sort_state = NOT_SORTED;
            gui.game_selector.title_sort_state = static_cast<gui::SortState>(std::max<int>(1, (gui.game_selector.title_sort_state + 1) % 3));
            gui.game_selector.is_game_list_sorted = true;
            switch (gui.game_selector.title_sort_state) {
            case ASCENDANT:
                std::sort(gui.game_selector.games.begin(), gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
                });
                break;
            case DESCENDANT:
                std::sort(gui.game_selector.games.begin(), gui.game_selector.games.end(), [](const Game &lhs, const Game &rhs) {
                    return string_utils::toupper(lhs.title) > string_utils::toupper(rhs.title);
                });
                break;
            default:
                break;
            }
        }
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_SEARCH_BAR_TEXT);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_COLOR_SEARCH_BAR_BG);
        ImGui::SameLine(ImGui::GetColumnWidth() - (ImGui::CalcTextSize("##refresh_game_list_button").x + ImGui::GetStyle().DisplayWindowPadding.x + 260));
        if (ImGui::Button("Refresh game list"))
            refresh_game_list(gui, host);
        ImGui::SameLine(ImGui::GetColumnWidth() - (ImGui::CalcTextSize("Game Search").x + ImGui::GetStyle().DisplayWindowPadding.x + 220));
        ImGui::TextColored(GUI_COLOR_TEXT, "Game Search");
        ImGui::SameLine();
        gui.game_search_bar.Draw("##game_search_bar", 220);
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        for (const auto &game : gui.game_selector.games) {
            bool selected[4] = { false, false, false, false };
            if (!gui.game_search_bar.PassFilter(game.title.c_str()) && !gui.game_search_bar.PassFilter(game.title_id.c_str()))
                continue;
            if (!fs::exists(fs::path(host.pref_path) / "ux0/app" / game.title_id)) {
                host.io.title_id = game.title_id;
                LOG_ERROR("Game not found: {} [{}], deleting the entry for it.", game.title_id, game.title);
                delete_game(gui, host);
            }
            if (gui.game_selector.icons.find(game.title_id) != gui.game_selector.icons.end()) {
                ImGui::Image(gui.game_selector.icons[game.title_id], ImVec2(icon_size, icon_size));
                if (ImGui::IsItemHovered()) {
                    host.io.title_id = game.title_id;
                    host.game_title = game.title;
                    if (host.cfg.show_game_background) {
                        if (gui.game_backgrounds.find(game.title_id) == gui.game_backgrounds.end())
                            load_game_background(gui, host);
                        else if (gui.current_background != gui.game_backgrounds[game.title_id])
                            gui.current_background = gui.game_backgrounds[game.title_id];
                    }
                    if (ImGui::IsMouseClicked(0))
                        selected[0] = true;
                }
                ImGui::OpenPopupOnItemClick("#game_context_menu", 1);
            }
            ImGui::NextColumn();
            ImGui::Selectable(game.title_id.c_str(), &selected[1], ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, icon_size));
            if (ImGui::IsItemHovered()) {
                host.io.title_id = game.title_id;
                host.game_title = game.title;
            }
            if (host.io.title_id == game.title_id)
                game_context_menu(gui, host);
            ImGui::NextColumn();
            ImGui::Selectable(game.app_ver.c_str(), &selected[2], ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, icon_size));
            ImGui::NextColumn();
            if (ImGui::IsItemHovered() && (gui.current_background != gui.user_backgrounds[host.cfg.background_image]))
                gui.current_background = gui.user_backgrounds[host.cfg.background_image];
            ImGui::Selectable(game.title.c_str(), &selected[3], ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, static_cast<float>(icon_size)));
            ImGui::NextColumn();
            if (std::find(std::begin(selected), std::end(selected), true) != std::end(selected)) {
                if (host.cfg.show_live_area_screen) {
                    init_live_area(gui, host);
                    gui.live_area.live_area_dialog = true;
                } else
                    gui.game_selector.selected_title_id = game.title_id;
            }
        }
        ImGui::PopStyleColor(4);
        break;
    }
    ImGui::End();
}

} // namespace gui
