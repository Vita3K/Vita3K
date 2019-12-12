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

    std::string fname = fmt::format("changeinfo_{:0>2d}.xml", host.cfg.sys_lang);

    if (!fs::exists(change_info_path / fname))
        fname = "changeinfo.xml";

    std::string xml_path = change_info_path.string() + fname;
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(xml_path.c_str());

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

enum GameInformation {
    NAME,
    TROPHY,
    PARENTAL,
    UPDATED,
    SIZE,
    VERSION
};

static std::map<int, std::pair<std::size_t, std::string>> game_info;

static bool get_game_info(GuiState &gui, HostState &host) {
    game_info.clear();
    const auto game_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id };

    if (fs::exists(game_path) && !fs::is_empty(game_path)) {
        time_t updated = fs::last_write_time(game_path);
        game_info[UPDATED].second = std::asctime(std::localtime(&updated));

        fs::recursive_directory_iterator end;
        for (fs::recursive_directory_iterator g(game_path); g != end; ++g) {
            const fs::path file = *g;
            if (fs::is_regular_file(file))
                game_info[SIZE].first += fs::file_size(file);
        }
        game_info[SIZE].first /= 1048576;
    }

    vfs::FileBuffer params;
    if (vfs::read_app_file(params, host.pref_path, host.io.title_id, "sce_sys/param.sfo")) {
        SfoFile sfo_handle;
        sfo::load(sfo_handle, params);
        sfo::get_data_by_key(game_info[NAME].second, sfo_handle, "TITLE");
        sfo::get_data_by_key(game_info[VERSION].second, sfo_handle, "APP_VER");
        sfo::get_data_by_key(game_info[PARENTAL].second, sfo_handle, "PARENTAL_LEVEL");
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

static auto delete_game_popup = false;
static auto delete_savedata_popup = false;
static auto check_savedata_and_shaderlog = false;
static auto update_history = false;
static auto information = false;

void game_context_menu(GuiState &gui, HostState &host) {
    const auto game_path{ fs::path(host.pref_path) / "ux0/app" / host.io.title_id };
    const auto save_data_path{ fs::path(host.pref_path) / "ux0/user/00/savedata" / host.io.title_id };
    const auto shaderlog_path{ fs::path(host.base_path) / "shaderlog" / host.io.title_id };

    // Context Game Menu
    if (ImGui::BeginPopupContextItem("#game_context_menu")) {
        if (ImGui::MenuItem("Boot", host.game_title.c_str()))
            gui.game_selector.selected_title_id = host.io.title_id;
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
            if (ImGui::MenuItem("Game")) {
                std::ostringstream game_explorer;
                game_explorer << OS_PREFIX << game_path.string();
                system(game_explorer.str().c_str());
            }
            if (fs::exists(save_data_path) && ImGui::MenuItem("Save Data")) {
                std::ostringstream save_data_explorer;
                save_data_explorer << OS_PREFIX << save_data_path.string();
                system(save_data_explorer.str().c_str());
            }
            ImGui::EndMenu();
        }
        if (!host.cfg.show_live_area_screen && ImGui::MenuItem("Live Area")) {
            init_live_area(gui, host);
            gui.live_area.live_area_dialog = true;
        }
        if (ImGui::BeginMenu("Delete")) {
            if (ImGui::MenuItem("Game"))
                delete_game_popup = true;
            if (fs::exists(save_data_path) && ImGui::MenuItem("Save Data"))
                delete_savedata_popup = true;
            if (ImGui::MenuItem("Shader Log")) {
                fs::remove_all(shaderlog_path);
            }
            ImGui::EndMenu();
        }
        if (fs::exists(game_path / "sce_sys/changeinfo/") && ImGui::MenuItem("Update history")) {
            if (get_update_history(gui, host))
                update_history = true;
            else
                LOG_WARN("Patch note Error for title: {}", host.io.title_id);
        }
        if (ImGui::MenuItem("Information") && get_game_info(gui, host))
            information = true;
        ImGui::EndPopup();
    }

    static const auto BUTTON_SIZE = ImVec2(60.f, 0.f);
    static const auto ICON_SIZE = ImVec2(128.f, 128.f);

    // Delete Game Popup
    if (delete_game_popup)
        ImGui::OpenPopup("Delete Game");
    if (ImGui::BeginPopupModal("Delete Game", &delete_game_popup, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        if (gui.game_selector.icons.find(host.io.title_id) != gui.game_selector.icons.end()) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2) - (ICON_SIZE.x / 2));
            ImGui::Image(gui.game_selector.icons[host.io.title_id], ICON_SIZE);
        }
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2) - (host.game_title.size() * 3));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", host.game_title.c_str());
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Do you really want to delete this application?");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Deleting a game may take a while!\nDepending on the size of the games and your hardware.");
        ImGui::Checkbox("Also delete save data and shader log for this game?", &check_savedata_and_shaderlog);
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 65);
        if (ImGui::Button("Ok", BUTTON_SIZE)) {
            if (check_savedata_and_shaderlog) {
                fs::remove_all(shaderlog_path);
                fs::remove_all(save_data_path);
                check_savedata_and_shaderlog = false;
            }
            delete_game(gui, host);
            delete_game_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", BUTTON_SIZE)) {
            delete_game_popup = false;
            if (check_savedata_and_shaderlog)
                check_savedata_and_shaderlog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Delete Save Data Popup
    if (delete_savedata_popup)
        ImGui::OpenPopup("Delete Save Data");
    if (ImGui::BeginPopupModal("Delete Save Data", &delete_savedata_popup, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        if (gui.game_selector.icons.find(host.io.title_id) != gui.game_selector.icons.end()) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2) - (ICON_SIZE.x / 2));
            ImGui::Image(gui.game_selector.icons[host.io.title_id], ICON_SIZE);
        }
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2) - (host.game_title.size() * 3));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", host.game_title.c_str());
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Do you really want to delete the save data for this application?");
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 65);
        if (ImGui::Button("Ok", BUTTON_SIZE)) {
            fs::remove_all(save_data_path);
            delete_savedata_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", BUTTON_SIZE)) {
            delete_savedata_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Information
    if (information)
        ImGui::OpenPopup("Information");
    if (ImGui::BeginPopupModal("Information", &information, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
        if (ImGui::Button("X", BUTTON_SIZE))
            information = false;
        if (gui.game_selector.icons.find(host.io.title_id) != gui.game_selector.icons.end()) {
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2) - (ICON_SIZE.x / 2));
            ImGui::Image(gui.game_selector.icons[host.io.title_id], ICON_SIZE);
        }
        ImGui::PushItemWidth(360.0f);
        if (ImGui::ListBoxHeader("##information", (int)game_info.size())) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 300.0f);
            ImGui::TextColored(GUI_COLOR_TEXT, "Name  %s", game_info[NAME].second.c_str());
            ImGui::TextColored(GUI_COLOR_TEXT, "Parental Controls  Level %d", *reinterpret_cast<const uint16_t *>(game_info[PARENTAL].second.c_str()));
            ImGui::TextColored(GUI_COLOR_TEXT, "Updated  %s", game_info[UPDATED].second.c_str());
            ImGui::TextColored(GUI_COLOR_TEXT, "Size  %d MB", game_info[SIZE].first);
            ImGui::TextColored(GUI_COLOR_TEXT, "Version  %s", game_info[VERSION].second.c_str());

            ImGui::PopTextWrapPos();
            ImGui::ListBoxFooter();
        }
        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }

    // Update History
    if (update_history)
        ImGui::OpenPopup("Update History");
    if (ImGui::BeginPopupModal("Update History", &update_history, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushItemWidth(400.0f);
        if (ImGui::ListBoxHeader("##update_history_list", (int)update_history_infos[host.io.title_id].first.size(), 8)) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 360.0f);
            for (auto u = 0; u < update_history_infos[host.io.title_id].first.size(); u++) {
                ImGui::TextColored(GUI_COLOR_TEXT, "Version %.2f", update_history_infos[host.io.title_id].first[u]);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", update_history_infos[host.io.title_id].second[u].c_str());
                ImGui::TextColored(GUI_COLOR_TEXT, "\n");
            }
            ImGui::PopTextWrapPos();
            ImGui::ListBoxFooter();
        }
        ImGui::PopItemWidth();
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
        if (ImGui::Button("Ok", BUTTON_SIZE))
            update_history = false;

        ImGui::EndPopup();
    }
}

} // namespace gui
