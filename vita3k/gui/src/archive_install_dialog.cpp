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

#include <util/string_utils.h>

#include <nfd.h>

#include <thread>

namespace gui {

static bool delete_archive_file;
static nfdchar_t *archive_path;

void draw_archive_install_dialog(GuiState &gui, HostState &host) {
    nfdresult_t result = NFD_CANCEL;

    static bool draw_file_dialog = true;
    static bool content_install_confirm = false;
    static bool finished_installing = false;
    static float progress;
    static bool reinstalling;

    if (draw_file_dialog) {
        result = NFD_OpenDialog("vpk,zip", nullptr, &archive_path);
        draw_file_dialog = false;
        finished_installing = false;

        if (result == NFD_OKAY) {
            std::thread installation([&host, &gui]() {
                if (install_archive(host, &gui, fs::path(string_utils::utf_to_wide(archive_path)), &progress))
                    content_install_confirm = true;
                else if (!gui.content_reinstall_confirm)
                    ImGui::OpenPopup("Content installation failed");
                finished_installing = true;
            });
            installation.detach();
        } else {
            gui.file_menu.archive_install_dialog = false;
            draw_file_dialog = true;
        }
    }

    if (reinstalling) {
        finished_installing = false;
        std::thread reinstallation([&host, &gui]() {
            if (install_archive(host, &gui, fs::path(string_utils::utf_to_wide(archive_path)), &progress)) {
                content_install_confirm = true;
            } else if (!gui.content_reinstall_confirm)
                ImGui::OpenPopup("Content installation failed");
            finished_installing = true;
            gui.content_reinstall_confirm = false;
        });
        reinstalling = false;
        reinstallation.detach();
    }

    if (!finished_installing) {
        ImGui::OpenPopup("Content Installation");
        if (ImGui::BeginPopupModal("Content Installation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(GUI_COLOR_TEXT, "Installation in progress, please wait...");
            ImGui::SetCursorPosX((ImGui::GetWindowContentRegionWidth() / 2) - (150 / 2) + 10);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::ProgressBar(progress / 100.f, ImVec2(150.f, 20.f), nullptr);
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }

    static const auto BUTTON_SIZE = ImVec2(60.f, 0.f);

    if (gui.content_reinstall_confirm && finished_installing)
        ImGui::OpenPopup("Content reinstallation");
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginPopupModal("Content reinstallation", &gui.content_reinstall_confirm, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::TextColored(GUI_COLOR_TEXT, "This content is already installed:\n%s [%s].", host.app_title_id.c_str(), host.app_title.c_str());
        if (!gui.app_ver.empty() && gui.app_ver != host.app_version)
            ImGui::TextColored(GUI_COLOR_TEXT, "Update content version from: %s to: %s.", gui.app_ver.c_str(), host.app_version.c_str());
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Do you want to reinstall it?");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 65);
        if (ImGui::Button("Yes", BUTTON_SIZE)) {
            reinstalling = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("No", BUTTON_SIZE)) {
            archive_path = nullptr;
            gui.content_reinstall_confirm = false;
            gui.file_menu.archive_install_dialog = false;
            draw_file_dialog = true;
        }
        ImGui::EndPopup();
    } else
        ImGui::PopStyleColor();

    if (content_install_confirm)
        ImGui::OpenPopup("Content installation success");
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginPopupModal("Content installation success", &content_install_confirm, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 5);
        ImGui::TextColored(GUI_COLOR_TEXT, "Successfully installed content.\n%s [%s] %s", host.app_title_id.c_str(), host.app_title.c_str(), host.app_version.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Checkbox("Delete the archive for the content installed?", &delete_archive_file);
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
        if (ImGui::Button("Ok", BUTTON_SIZE)) {
            if (delete_archive_file) {
                fs::remove(fs::path(string_utils::utf_to_wide(archive_path)));
                delete_archive_file = false;
            }
            content_install_confirm = false;
            draw_file_dialog = true;
            archive_path = nullptr;
            if (host.app_category == "gd")
                refresh_app_list(gui, host);
            gui.file_menu.archive_install_dialog = false;
        }
        ImGui::EndPopup();
    } else
        ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_MENUBAR);
    if (ImGui::BeginPopupModal("Content installation failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::TextColored(GUI_COLOR_TEXT, "Installation failed, please check the log for more information.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Checkbox("Delete the archive for the content that failed to install?", &delete_archive_file);
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
        if (ImGui::Button("Ok", BUTTON_SIZE)) {
            if (delete_archive_file) {
                fs::remove(fs::path(string_utils::utf_to_wide(archive_path)));
                delete_archive_file = false;
            }
            draw_file_dialog = true;
            archive_path = nullptr;
            gui.file_menu.archive_install_dialog = false;
        }
        ImGui::EndPopup();
    } else
        ImGui::PopStyleColor();
}

} // namespace gui
