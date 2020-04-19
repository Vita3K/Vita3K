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

#include <host/functions.h>
#include <host/state.h>

#include <io/vfs.h>

#include <util/log.h>
#include <util/string_utils.h>

#include <pugixml.hpp>
#include <sstream>

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

static std::map<std::string, std::pair<std::vector<double>, std::vector<std::string>>> update_history_infos;

static bool get_update_history(GuiState &gui, HostState &host) {
    const auto change_info_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id / "sce_sys/changeinfo/" };

    std::string fname = fs::exists(change_info_path / fmt::format("changeinfo_{:0>2d}.xml", host.cfg.sys_lang)) ? fmt::format("changeinfo_{:0>2d}.xml", host.cfg.sys_lang) : "changeinfo.xml";

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file((change_info_path.string() + fname).c_str());

    for (const auto &update : doc.child("changeinfo")) {
        update_history_infos[host.io.title_id].first.push_back({ update.attribute("app_ver").as_double() });
        update_history_infos[host.io.title_id].second.push_back({ update.text().as_string() });
    }

    for (auto &i : update_history_infos[host.io.title_id].second) {
        const auto startpos = "<";
        const auto endpos = ">";

        if (i.find_first_of('\n') != std::string::npos)
            i.erase(i.begin() + i.find_first_of('\n'));

        while (i.find("</li> <li>") != std::string::npos)
            if (i.find("</li> <li>") != std::string::npos)
                i.replace(i.find("</li> <li>"), i.find(endpos) + 2 - i.find(startpos), "\n");
        while (i.find("<li>") != std::string::npos)
            if (i.find("<li>") != std::string::npos)
                i.replace(i.find("<li>"), i.find(endpos) + 1 - i.find(startpos), ". ");
        while (i.find(startpos) != std::string::npos)
            if (i.find(">") + 1 != std::string::npos)
                i.erase(i.find(startpos), i.find(endpos) + 1 - i.find(startpos));

        bool found_space = false;
        auto end{ std::remove_if(i.begin(), i.end(), [&found_space](unsigned ch) {
            bool is_space = std::isspace(ch);
            std::swap(found_space, is_space);
            return found_space && is_space;
        }) };

        if (end != i.begin() && std::isspace(static_cast<unsigned>(end[-1])))
            --end;

        i.erase(end, i.end());
    }

    return !update_history_infos[host.io.title_id].first.empty() && !update_history_infos[host.io.title_id].second.empty();
}

static std::map<std::string, std::string> game_info;

static bool get_game_info(GuiState &gui, HostState &host) {
    game_info.clear();
    const auto game_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id };

    if (fs::exists(game_path) && !fs::is_empty(game_path)) {
        game_info["trophy"] = fs::exists(game_path / "sce_sys/trophy") ? "Eligible" : "Ineligible";

        time_t updated = fs::last_write_time(game_path);
        game_info["updated"] = std::asctime(std::localtime(&updated));

        size_t game_size = 0;
        fs::recursive_directory_iterator end;
        for (fs::recursive_directory_iterator g(game_path); g != end; ++g) {
            const fs::path file = *g;
            if (fs::is_regular_file(file))
                game_size += fs::file_size(file);
        }
        game_info["size"] = std::to_string(game_size / 1048576);
    }

    vfs::FileBuffer params;
    if (vfs::read_app_file(params, host.pref_path, host.io.title_id, "sce_sys/param.sfo")) {
        SfoFile sfo_handle;
        sfo::load(sfo_handle, params);
        sfo::get_data_by_key(game_info["parental"], sfo_handle, "PARENTAL_LEVEL");
    }

    return !game_info.empty();
}

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

static std::string context_dialog;
static auto check_savedata_and_shaderlog = false;
static auto information = false;

void game_context_menu(GuiState &gui, HostState &host) {
    const auto game_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id };
    const auto save_data_path{ fs::path(host.pref_path) / "ux0/user/00/savedata" / host.io.title_id };
    const auto shaderlog_path{ fs::path(host.base_path) / "shaderlog" / host.io.title_id };

    // Game Context Menu
    if (ImGui::BeginPopupContextItem("#game_context_menu")) {
        if (ImGui::MenuItem("Boot", host.game_title.c_str()))
            gui.game_selector.selected_title_id = host.io.title_id;
        if (ImGui::MenuItem("Check Game Compatibility")) {
            const std::string compat_url = host.io.title_id.find("PCS") != std::string::npos ? "https://vita3k.org/compatibility?g=" + host.io.title_id : "https://github.com/Vita3K/homebrew-compatibility/issues?q=" + host.game_title;
            system((OS_PREFIX + compat_url).c_str());
        }
        if (ImGui::BeginMenu("Copy game info")) {
            if (ImGui::MenuItem("ID and Name")) {
                ImGui::LogToClipboard();
                ImGui::LogText("%s [%s]", host.io.title_id.c_str(), host.game_title.c_str());
                ImGui::LogFinish();
            }
            if (ImGui::MenuItem("ID")) {
                ImGui::LogToClipboard();
                ImGui::LogText("%s", host.io.title_id.c_str());
                ImGui::LogFinish();
            }
            if (ImGui::MenuItem("Name")) {
                ImGui::LogToClipboard();
                ImGui::LogText("%s", host.game_title.c_str());
                ImGui::LogFinish();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Open Folder")) {
            if (ImGui::MenuItem("Game"))
                system((OS_PREFIX + game_path.string()).c_str());
            if (fs::exists(save_data_path) && ImGui::MenuItem("Save Data"))
                system((OS_PREFIX + save_data_path.string()).c_str());
            ImGui::EndMenu();
        }
        if (!host.cfg.show_live_area_screen && ImGui::MenuItem("Live Area", nullptr, &gui.live_area.live_area_dialog))
            init_live_area(gui, host);
        if (ImGui::BeginMenu("Delete")) {
            if (ImGui::MenuItem("Game"))
                context_dialog = "game";
            if (fs::exists(save_data_path) && ImGui::MenuItem("Save Data"))
                context_dialog = "save";
            if (ImGui::MenuItem("Shader Log")) {
                fs::remove_all(shaderlog_path);
            }
            ImGui::EndMenu();
        }
        if (fs::exists(game_path / "sce_sys/changeinfo/") && ImGui::MenuItem("Update history")) {
            if (get_update_history(gui, host))
                context_dialog = "history";
            else
                LOG_WARN("Patch note Error for title: {}", host.io.title_id);
        }
        ImGui::MenuItem("Information", nullptr, &information) && get_game_info(gui, host);
        ImGui::EndPopup();
    }

    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto scal = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);
    const auto WINDOW_SIZE = ImVec2(756.0f * scal.x, 436.0f * scal.y);

    const auto BUTTON_SIZE = ImVec2(320.f * scal.x, 46.f * scal.y);
    const auto ICON_SIZE = ImVec2(100.f * scal.x, 100.f * scal.y);

    // Context Dialog
    if (!context_dialog.empty()) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##context_dialog", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextWindowPosCenter();
        ImGui::BeginChild("##context_dialog_child", WINDOW_SIZE, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        // Update History
        if (context_dialog == "history") {
            ImGui::SetCursorPos(ImVec2(20.f * scal.x, BUTTON_SIZE.y));
            if (ImGui::ListBoxHeader("##update_history_list", ImVec2(WINDOW_SIZE.x - (40.f * scal.x), WINDOW_SIZE.y - (BUTTON_SIZE.y * 2.f) - (25.f * scal.y)))) {
                for (auto u = 0; u < update_history_infos[host.io.title_id].first.size(); u++) {
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::TextColored(GUI_COLOR_TEXT, "Version %.2f", update_history_infos[host.io.title_id].first[u]);
                    ImGui::SetWindowFontScale(1.4f);
                    ImGui::PushTextWrapPos(WINDOW_SIZE.x - (80.f * scal.x));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s\n", update_history_infos[host.io.title_id].second[u].c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::TextColored(GUI_COLOR_TEXT, "\n");
                }
                ImGui::ListBoxFooter();
            }
            ImGui::SetWindowFontScale(1.4f * scal.x);
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BUTTON_SIZE.y - (22.f * scal.y)));
        } else {
            // Delete Data
            if (gui.game_selector.icons.find(host.io.title_id) != gui.game_selector.icons.end()) {
                ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ICON_SIZE.x / 2.f));
                ImGui::Image(gui.game_selector.icons[host.io.title_id], ICON_SIZE);
            }
            ImGui::SetWindowFontScale(1.4f * scal.x);
            ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(host.game_short_title.c_str()).x / 2.f));
            ImGui::TextColored(GUI_COLOR_TEXT, host.game_short_title.c_str());
            std::string ask_delete = context_dialog == "save" ? "Do you really want to delete this save data for this application?" : "Do you really want to delete this application?";
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(ask_delete.c_str()).x / 2, (WINDOW_SIZE.y / 2) + 10));
            ImGui::TextColored(GUI_COLOR_TEXT, ask_delete.c_str());
            if (context_dialog == "game") {
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Deleting a game may take a while\ndepending on its size and your hardware.");
                std::string checkbox_delete = "Also delete save data and shader log for this game?";
                ImGui::SetCursorPosX((WINDOW_SIZE.x / 2) - (ImGui::CalcTextSize(checkbox_delete.c_str()).x / 2) - 36.f);
                ImGui::Checkbox(checkbox_delete.c_str(), &check_savedata_and_shaderlog);
            }
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * scal.x)), WINDOW_SIZE.y - BUTTON_SIZE.y - (22.0f * scal.y)));
            if (ImGui::Button("Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
                check_savedata_and_shaderlog = false;
                context_dialog.clear();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 + 20.f);
        }
        if (ImGui::Button("Ok", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
            if (context_dialog == "game") {
                if (check_savedata_and_shaderlog) {
                    fs::remove_all(shaderlog_path);
                    fs::remove_all(save_data_path);
                    check_savedata_and_shaderlog = false;
                }
                delete_game(gui, host);
            } else if (context_dialog == "save")
                fs::remove_all(save_data_path);
            context_dialog.clear();
        }
        ImGui::EndChild();
        ImGui::End();
    }

    // Information
    if (information) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##information", &information, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(1.4f * scal.x);
        ImGui::SetCursorPos(ImVec2(10.0f * scal.x, 10.0f * scal.y));
        if (ImGui::Button("X", ImVec2(40.f * scal.x, 40.f * scal.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle))
            information = false;
        if (gui.game_selector.icons.find(host.io.title_id) != gui.game_selector.icons.end()) {
            ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (ICON_SIZE.x / 2.f), 40.f * scal.y));
            ImGui::Image(gui.game_selector.icons[host.io.title_id], ICON_SIZE);
        }
        const auto calc_name = ImGui::CalcTextSize("Name  ");
        const auto calc_title = ImGui::CalcTextSize(host.game_title.c_str(), 0, false, 200.f * scal.x).y;
        ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - calc_name.x, ((ICON_SIZE.y * 2.4f) + (calc_title / 2.f)) - ((calc_title / 2.f) + (calc_name.y / 2.f))));
        ImGui::TextColored(GUI_COLOR_TEXT, "Name ");
        ImGui::SetCursorPos(ImVec2(display_size.x / 2.f, ((ICON_SIZE.y * 2.4f) + (calc_title / 2.f)) - calc_title));
        ImGui::PushTextWrapPos(display_size.x - (280.f * scal.x));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", host.game_title.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize("Trophy Earning  ").x);
        ImGui::TextColored(GUI_COLOR_TEXT, "Trophy Earning  %s", game_info["trophy"].c_str());
        ImGui::Spacing();
        ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize("Parental Controls  ").x);
        ImGui::TextColored(GUI_COLOR_TEXT, "Parental Controls  Level %d", *reinterpret_cast<const uint16_t *>(game_info["parental"].c_str()));
        ImGui::Spacing();
        ImGui::SetCursorPosX(((display_size.x / 2.f) - ImGui::CalcTextSize("Updated  ").x));
        ImGui::TextColored(GUI_COLOR_TEXT, "Updated  %s", game_info["updated"].c_str());
        ImGui::Spacing();
        ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize("Size  ").x);
        ImGui::TextColored(GUI_COLOR_TEXT, "Size  %s MB", game_info["size"].c_str());
        ImGui::Spacing();
        ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize("Version  ").x);
        ImGui::TextColored(GUI_COLOR_TEXT, "Version  %s", host.game_version.c_str());
        ImGui::End();
    }
}

} // namespace gui
