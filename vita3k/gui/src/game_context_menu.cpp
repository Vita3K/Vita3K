// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

static bool delete_game_popup = false;
static bool delete_savedata_popup = false;
static bool check_savedata_and_shaderlog = false;

void delete_game(GuiState &gui, HostState &host, const std::string &title_id) {
    const fs::path game_path{ fs::path(host.pref_path) / "ux0/app" };

    for (const auto &game : gui.game_selector.games) {
        const auto games_index = std::find_if(gui.game_selector.games.begin(), gui.game_selector.games.end(), [&game](const Game &g) {
            return g.app_ver == game.app_ver && g.title == game.title && g.title_id == game.title_id;
        });

        if (title_id == game.title_id) {
            LOG_INFO_IF(fs::remove_all(game_path / game.title_id), "Successfully deleted '{} [{}]'.", game.title_id, game.title);

            if (!fs::exists(game_path / game.title_id)) {
                gui.delete_game_background = true;
                gui.delete_game_icon = true;
                gui.game_selector.games.erase(games_index);
            } else
                LOG_ERROR("Failed to delete '{} [{}]'.", game.title_id, game.title);
        }
    }
}

void game_context_menu(GuiState &gui, HostState &host, bool *selected, const std::string &title_id) {
    const fs::path save_data_path{ fs::path(host.pref_path) / "ux0/user/00/savedata" };
    const fs::path shaderlog_path{ fs::path(host.base_path) / "shaderlog" };

    for (const auto &game : gui.game_selector.games) {
        if (title_id == game.title_id) {
            // Context Game Menu
            if (ImGui::BeginPopupContextItem("#game_context_menu")) {
                if (ImGui::MenuItem("Boot", game.title.c_str()))
                    selected[0] = true;
                if (ImGui::BeginMenu("Copy game info")) {
                    if (ImGui::MenuItem("ID and Name")) {
                        ImGui::LogToClipboard();
                        ImGui::LogText("%s [%s]", game.title_id.c_str(), game.title.c_str());
                        ImGui::LogFinish();
                    }
                    if (ImGui::MenuItem("ID")) {
                        ImGui::LogToClipboard();
                        ImGui::LogText("%s", game.title_id.c_str());
                        ImGui::LogFinish();
                    }
                    if (ImGui::MenuItem("Name")) {
                        ImGui::LogToClipboard();
                        ImGui::LogText("%s", game.title.c_str());
                        ImGui::LogFinish();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Remove")) {
                    if (ImGui::MenuItem("Game"))
                        delete_game_popup = true;
                    if (ImGui::MenuItem("Save Data"))
                        delete_savedata_popup = true;
                    if (ImGui::MenuItem("Shader Log")) {
                        fs::remove_all(shaderlog_path / game.title_id);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            static const auto BUTTON_SIZE = ImVec2(60.f, 0.f);

            // Delete Game Popup
            if (delete_game_popup)
                ImGui::OpenPopup("Delete Game");
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
            if (ImGui::BeginPopupModal("Delete Game", &delete_game_popup, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::PopStyleColor();
                ImGui::TextColored(GUI_COLOR_TEXT, "Do you really want to delete this game?\n%s [%s] %s", game.title_id.c_str(), game.title.c_str(), game.app_ver.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Deleting a game may take a while!\nDepending on the size of the games and your hardware.");
                ImGui::Checkbox("Also delete save data and shader log for this game?", &check_savedata_and_shaderlog);
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 65);
                if (ImGui::Button("Yes", BUTTON_SIZE)) {
                    if (check_savedata_and_shaderlog) {
                        fs::remove_all(shaderlog_path / game.title_id);
                        fs::remove_all(save_data_path / game.title_id);
                        check_savedata_and_shaderlog = false;
                    }
                    delete_game(gui, host, title_id);
                    delete_game_popup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No", BUTTON_SIZE)) {
                    delete_game_popup = false;
                    if (check_savedata_and_shaderlog)
                        check_savedata_and_shaderlog = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            } else
                ImGui::PopStyleColor();

            // Delete Save Data Popup
            if (delete_savedata_popup)
                ImGui::OpenPopup("Delete Save Data");
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
            if (ImGui::BeginPopupModal("Delete Save Data", &delete_savedata_popup, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::PopStyleColor();
                if (fs::exists(save_data_path / game.title_id)) {
                    ImGui::TextColored(GUI_COLOR_TEXT, "Do you really want to delete the save data for this game?\n%s [%s]", game.title_id.c_str(), game.title.c_str());
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 65);
                    if (ImGui::Button("Yes", BUTTON_SIZE)) {
                        fs::remove_all(save_data_path / game.title_id);
                        delete_savedata_popup = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("No", BUTTON_SIZE)) {
                        delete_savedata_popup = false;
                        ImGui::CloseCurrentPopup();
                    }
                } else {
                    ImGui::TextColored(GUI_COLOR_TEXT, "Save data not found for:\n%s [%s]", game.title_id.c_str(), game.title.c_str());
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
                    if (ImGui::Button("Ok", BUTTON_SIZE)) {
                        delete_savedata_popup = false;
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            } else
                ImGui::PopStyleColor();
        }
    }
}

} // namespace gui
