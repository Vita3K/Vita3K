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

#include <config/functions.h>
#include <host/state.h>
#include <misc/cpp/imgui_stdlib.h>
#include <util/log.h>

namespace gui {

void draw_profiles_manager_dialog(GuiState &gui, HostState &host) {
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    ImGui::Begin("Profiles Manager", &gui.configuration_menu.profiles_manager_dialog, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleColor();

    const auto update_online_id = std::find(host.cfg.online_id.begin(), host.cfg.online_id.end(), gui.online_id);
    static const fs::path user_path{ fs::path(host.pref_path) / "ux0/user" };
    static const auto BUTTON_SIZE = ImVec2(60.f, 0.f);

    // Add user
    ImGui::InputTextWithHint("##online_id", "Write your online ID", &gui.online_id);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Set your Online ID with a maximum of 16 characters.");
    ImGui::SameLine();
    if (ImGui::Button("Add Online ID") && !gui.online_id.empty())
        ImGui::OpenPopup("Add Online ID");
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginPopupModal("Add Online ID", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();

        if (gui.online_id.length() > 16)
            ImGui::TextColored(GUI_COLOR_TEXT, "Error: Need reduce to length for online ID.\nCurrently the length is: %d.", gui.online_id.length());
        else if (update_online_id != host.cfg.online_id.end())
            ImGui::TextColored(GUI_COLOR_TEXT, "Error: '%s' Online ID already exists.", gui.online_id.c_str());
        else
            ImGui::TextColored(GUI_COLOR_TEXT, "Success: '%s' Online ID added.", gui.online_id.c_str());

        ImGui::Separator();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
        if (ImGui::Button("OK", BUTTON_SIZE)) {
            if (update_online_id == host.cfg.online_id.end() && !(gui.online_id.length() > 16)) {
                host.cfg.online_id.push_back(gui.online_id);
                host.cfg.user_id += ((int)host.cfg.online_id.size() - 1) - host.cfg.user_id;
                host.io.user_id = fmt::format("{:0>2d}", host.cfg.user_id);
                if (!fs::exists(user_path / host.io.user_id))
                    fs::create_directories(user_path / host.io.user_id / "savedata");
                gui.online_id.clear();
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else
        ImGui::PopStyleColor();

    // User list
    ImGui::PushItemWidth(300);
    if (ImGui::ListBoxHeader("##online_id_list", (int)host.cfg.online_id.size(), 4)) {
        for (auto i = 0; i < host.cfg.online_id.size(); i++) {
            ImGui::TextColored(GUI_COLOR_TEXT, "User ID: %02d, Online ID: %s", i, host.cfg.online_id[i].c_str());
        }
        ImGui::ListBoxFooter();
    }
    ImGui::PopItemWidth();
    ImGui::Spacing();
    if (host.cfg.online_id.size() > 1) {
        ImGui::PushItemWidth(210);
        ImGui::TextColored(GUI_COLOR_TEXT, "Select Your User.");
        ImGui::Combo(
            "##select_user", &host.cfg.user_id,
            [](void *vec, int idx, const char **list_online_id) {
                auto &vector = *static_cast<std::vector<std::string> *>(vec);
                if (idx < 0 || idx >= static_cast<int>(vector.size())) {
                    return false;
                }
                *list_online_id = vector.at(idx).c_str();
                return true;
            },
            static_cast<void *>(&host.cfg.online_id), static_cast<int>(host.cfg.online_id.size()));
        ImGui::SameLine();
        ImGui::TextColored(GUI_COLOR_TEXT, "User ID: %02d", host.cfg.user_id);
        ImGui::PopItemWidth();

        // Delete User
        ImGui::Spacing();
        if (ImGui::Button("Delete Selected User"))
            ImGui::OpenPopup("Delete User");
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
        if (ImGui::BeginPopupModal("Delete User", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PopStyleColor();
            const auto delete_online_id = std::find(host.cfg.online_id.begin(), host.cfg.online_id.end(), host.cfg.online_id[host.cfg.user_id]);

            if (fs::exists(user_path / host.io.user_id)) {
                ImGui::TextColored(GUI_COLOR_TEXT, "Do you want also delete folder for this user?\nUser ID: %s, Online ID: %s", host.io.user_id.c_str(), host.cfg.online_id[host.cfg.user_id].c_str());
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 100);
                if (ImGui::Button("Yes", BUTTON_SIZE)) {
                    fs::remove_all(user_path / host.io.user_id);
                    host.cfg.online_id.erase(delete_online_id);
                    if (host.cfg.user_id < static_cast<int>(host.cfg.online_id.size())) {
                        auto o = host.cfg.user_id + 1, n = host.cfg.user_id;
                        for (o, n; o < host.cfg.online_id.size() + 1 && n < host.cfg.online_id.size(); o++, n++) {
                            auto old_user_id = fmt::format("{:0>2d}", o);
                            auto new_user_id = fmt::format("{:0>2d}", n);
                            if (fs::exists(user_path / old_user_id) && !fs::exists(user_path / new_user_id))
                                fs::rename(user_path / old_user_id, user_path / new_user_id);
                        }
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("No", BUTTON_SIZE)) {
                    host.cfg.online_id.erase(delete_online_id);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", BUTTON_SIZE)) {
                    ImGui::CloseCurrentPopup();
                }

            } else {
                ImGui::TextColored(GUI_COLOR_TEXT, "Do you really want delete this user?\nUser ID: %s, Online ID: %s", host.io.user_id.c_str(), host.cfg.online_id[host.cfg.user_id].c_str());
                ImGui::Separator();
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 65);
                if (ImGui::Button("Yes", BUTTON_SIZE)) {
                    host.cfg.online_id.erase(delete_online_id);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No", BUTTON_SIZE)) {
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        } else
            ImGui::PopStyleColor();
    }

    // Rename User
    ImGui::SameLine();
    std::string rename_button = "Rename Main User";
    if (host.cfg.user_id > 0)
        rename_button = "Rename Selected User";
    if (ImGui::Button(rename_button.c_str())) {
        ImGui::OpenPopup("Rename Online ID");
        if (!gui.online_id.empty())
            gui.online_id.clear();
        gui.online_id = host.cfg.online_id[host.cfg.user_id];
    }
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginPopupModal("Rename Online ID", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::InputTextWithHint("##online_id", "Write change online ID", &gui.online_id);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Set your Online ID with a maximum of 16 characters.");
        ImGui::Separator();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 65);
        if (ImGui::Button("Apply", BUTTON_SIZE))
            ImGui::OpenPopup("Apply Rename Online ID");
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
        if (ImGui::BeginPopupModal("Apply Rename Online ID", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PopStyleColor();

            if (gui.online_id.empty())
                ImGui::TextColored(GUI_COLOR_TEXT, "Error: Text is empty, write you Online ID.");
            else if (gui.online_id.length() > 16)
                ImGui::TextColored(GUI_COLOR_TEXT, "Error: Need reduce to length for online ID.\nCurrently the length is: %d.", gui.online_id.length());
            else if (update_online_id != host.cfg.online_id.end())
                ImGui::TextColored(GUI_COLOR_TEXT, "Error: '%s' Online ID already exists.", gui.online_id.c_str());
            else
                ImGui::TextColored(GUI_COLOR_TEXT, "Success: '%s' Online ID renamed to '%s'.", host.cfg.online_id[host.cfg.user_id].c_str(), gui.online_id.c_str());

            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
            if (ImGui::Button("OK", BUTTON_SIZE)) {
                if (gui.online_id.empty())
                    gui.online_id = host.cfg.online_id[host.cfg.user_id];
                else if (update_online_id == host.cfg.online_id.end() && !(gui.online_id.length() > 16)) {
                    host.cfg.online_id[host.cfg.user_id].replace(host.cfg.online_id[host.cfg.user_id].begin(), host.cfg.online_id[host.cfg.user_id].end(), gui.online_id);
                    gui.online_id = host.cfg.online_id[host.cfg.user_id];
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        } else
            ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("Close", BUTTON_SIZE)) {
            gui.online_id.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else
        ImGui::PopStyleColor();

    if (static_cast<int>(host.cfg.online_id.size()) - 1 < host.cfg.user_id)
        --host.cfg.user_id;

    if (host.io.user_id.empty() || std::stoi(host.io.user_id) != host.cfg.user_id)
        host.io.user_id = fmt::format("{:0>2d}", host.cfg.user_id);

    if (host.cfg.overwrite_config)
        config::serialize_config(host.cfg, host.cfg.config_path);

    ImGui::End();
}

} // namespace gui
