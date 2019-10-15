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

#include "interface.h"
#include "private.h"

#include <gui/functions.h>
#include <gui/state.h>

#include <host/functions.h>
#include <host/sfo.h>
#include <host/state.h>

#include <io/vfs.h>

#include <util/log.h>

#include <nfd.h>

namespace gui {

nfdchar_t *game_path = nullptr;
static bool draw_file_dialog = true;
static bool game_install_confirm = false;

void draw_game_install_dialog(GuiState &gui, HostState &host) {
    nfdresult_t result = NFD_CANCEL;

    if (draw_file_dialog) {
        result = NFD_OpenDialog("vpk,zip", nullptr, &game_path);
        draw_file_dialog = false;

        if (result == NFD_OKAY) {
            if (install_vpk(host, gui, static_cast<fs::path>(game_path)))
                game_install_confirm = true;
            else if (!gui.game_reinstall_confirm)
                ImGui::OpenPopup("Game installation failed");
        } else {
            gui.file_menu.game_install_dialog = false;
            draw_file_dialog = true;
        }
    }

    static const auto BUTTON_SIZE = ImVec2(60.f, 0.f);

    if (gui.game_reinstall_confirm)
        ImGui::OpenPopup("Game reinstallation");
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    if (ImGui::BeginPopupModal("Game reinstallation", &gui.game_reinstall_confirm, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::TextColored(GUI_COLOR_TEXT, "This game is already installed:\n%s [%s].", host.io.title_id.c_str(), host.game_title.c_str());
        if (!gui.app_ver.empty() && gui.app_ver != host.game_version)
            ImGui::TextColored(GUI_COLOR_TEXT, "Update app version from: %s to: %s.", gui.app_ver.c_str(), host.game_version.c_str());
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Do you want to reinstall it?");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 65);
        if (ImGui::Button("Yes", BUTTON_SIZE)) {
            if (install_vpk(host, gui, static_cast<fs::path>(game_path))) {
                gui.game_reinstall_confirm = false;
                game_install_confirm = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("No", BUTTON_SIZE)) {
            gui.game_reinstall_confirm = false;
            gui.file_menu.game_install_dialog = false;
            draw_file_dialog = true;
        }
        ImGui::EndPopup();
    } else
        ImGui::PopStyleColor();

    if (game_install_confirm)
        ImGui::OpenPopup("Game installation succes");
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    if (ImGui::BeginPopupModal("Game installation succes", &game_install_confirm, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::TextColored(GUI_COLOR_TEXT, "Succesfully installed game.\n%s [%s] %s", host.io.title_id.c_str(), host.game_title.c_str(), host.game_version.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
        if (ImGui::Button("Ok", BUTTON_SIZE)) {
            game_install_confirm = false;
            draw_file_dialog = true;
            refresh_game_list(gui, host);
            gui.file_menu.game_install_dialog = false;
        }
        ImGui::EndPopup();
    } else
        ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR_OPTIONS);
    if (ImGui::BeginPopupModal("Game Installation Failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::TextColored(GUI_COLOR_TEXT, "Installation failed, please check the log for more information.");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
        if (ImGui::Button("Ok", BUTTON_SIZE)) {
            draw_file_dialog = true;
            gui.file_menu.game_install_dialog = false;
        }
        ImGui::EndPopup();
    } else
        ImGui::PopStyleColor();
}

} // namespace gui