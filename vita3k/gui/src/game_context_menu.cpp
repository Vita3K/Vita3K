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

#include <util/log.h>
#include <util/string_utils.h>

#include <SDL_keycode.h>

namespace gui {

void delete_game(GuiState &gui, HostState &host) {
    const fs::path game_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id };

    for (const auto &game : gui.game_selector.games) {
        const auto games_index = std::find_if(gui.game_selector.games.begin(), gui.game_selector.games.end(), [&game](const Game &g) {
            return g.app_ver == game.app_ver && g.title == game.title && g.title_id == game.title_id;
        });

        if (host.io.title_id == game.title_id) {
            LOG_INFO_IF(fs::remove_all(game_path), "Successfully deleted '{} [{}]'.", game.title_id, game.title);

            if (!fs::exists(game_path)) {
                gui.delete_game_background = true;
                gui.delete_game_icon = true;
                gui.game_selector.games.erase(games_index);
            } else
                LOG_ERROR("Failed to delete '{} [{}]'.", game.title_id, game.title);
        }
    }
}

static auto current_page_manual = 0;
static std::vector<std::string> manual_page_list;
static auto manual_page_count = 0;

static bool get_manual_list(GuiState &gui, HostState &host) {
    current_page_manual = 0;
    gui.manual.clear();
    manual_page_list.clear();

    auto manual_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id / "sce_sys/manual/" };
    if (fs::exists(manual_path / fmt::format("{:0>2d}", host.cfg.sys_lang)))
        manual_path /= fmt::format("{:0>2d}", host.cfg.sys_lang);
    if (fs::exists(manual_path) && !fs::is_empty(manual_path)) {
        fs::directory_iterator end;
        const std::string ext = ".png";
        for (fs::directory_iterator m(manual_path); m != end; ++m) {
            const fs::path manual = *m;
            if (m->path().extension() == ext)
                manual_page_list.push_back({ manual.filename().string() });
        }
        manual_page_count = (int)manual_page_list.size();
    }

    return !manual_page_list.empty();
}

static auto delete_game_popup = false;
static auto delete_savedata_popup = false;
static auto check_savedata_and_shaderlog = false;
static auto manual_popup = false;
static auto hiden_button = false;

void game_context_menu(GuiState &gui, HostState &host, bool *selected) {
    const fs::path save_data_path{ fs::path(host.pref_path) / "ux0/user/00/savedata" };
    const fs::path shaderlog_path{ fs::path(host.base_path) / "shaderlog" };

    for (const auto &game : gui.game_selector.games) {
        if (host.io.title_id == game.title_id) {
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
                if (fs::exists(fs::path(host.pref_path) / "ux0/app" / host.io.title_id / "sce_sys/manual") && ImGui::MenuItem("Manual")) {
                    if (get_manual_list(gui, host))
                        manual_popup = true;
                    else
                        LOG_WARN("Manual not found for title: {}", game.title_id);
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
                    delete_game(gui, host);
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

            // Manual Popup
            if (manual_popup)
                ImGui::OpenPopup("Manual");
            const auto display_size = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(-5.0f, -5.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(display_size.x + 10.0f, display_size.y + 10.0f), ImGuiCond_Always);
            if (ImGui::BeginPopupModal("Manual", &manual_popup, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
                if (gui.manual.find(manual_page_list[current_page_manual]) == gui.manual.end())
                    load_manual(gui, host, manual_page_list[current_page_manual]);
                else if (gui.current_page_manual != gui.manual[manual_page_list[current_page_manual]])
                    gui.current_page_manual = gui.manual[manual_page_list[current_page_manual]];

                if (gui.current_page_manual)
                    ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<void *>(gui.current_page_manual), ImVec2(0, 0), display_size);
                else
                    manual_popup = false;

                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (!hiden_button)
                        hiden_button = true;
                    else
                        hiden_button = false;
                }

                if (current_page_manual > 0) {
                    ImGui::SetCursorPos(ImVec2(10.0f, display_size.y - 30.0f));
                    if (!hiden_button && ImGui::Button("<", BUTTON_SIZE) || ImGui::IsKeyPressed(SDL_SCANCODE_LEFT))
                        --current_page_manual;
                }
                if (!hiden_button) {
                    ImGui::SameLine();
                    ImGui::SetCursorPos(ImVec2(display_size.x / 2 - 20.0f, display_size.y - 30.0f));
                    const std::string slider = fmt::format("{}/{}", current_page_manual + 1, manual_page_count);
                    if (ImGui::Button(slider.c_str(), BUTTON_SIZE))
                        ImGui::OpenPopup("Slider Manual");
                    ImGui::SetNextWindowPos(ImVec2(-5.0f, display_size.y - 50.0f), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(ImVec2(display_size.x + 10.0f, 40.0f), ImGuiCond_Always);
                    if (ImGui::BeginPopupModal("Slider Manual", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
                        ImGui::PushItemWidth(display_size.x - 10.0f);
                        ImGui::SliderInt("##slider_current_manual", &current_page_manual, 0, manual_page_count - 1);
                        ImGui::PopItemWidth();
                        if (ImGui::IsItemDeactivated())
                            ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                }
                if (current_page_manual < manual_page_count - 1) {
                    ImGui::SameLine();
                    ImGui::SetCursorPos(ImVec2(display_size.x - 60.0f, display_size.y - 30.0f));
                    if (!hiden_button && ImGui::Button(">", BUTTON_SIZE) || ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT))
                        ++current_page_manual;
                }

                ImGui::SetCursorPos(ImVec2(display_size.x - 60.0f, 20.0f));
                if (!hiden_button && ImGui::Button("X", BUTTON_SIZE) || ImGui::IsKeyPressed(SDL_SCANCODE_H))
                    manual_popup = false;

                ImGui::EndPopup();
            }
        }
    }
}

} // namespace gui
