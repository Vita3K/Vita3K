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
#include <host/functions.h>
#include <host/pkg.h>
#include <misc/cpp/imgui_stdlib.h>
#include <rif2zrif.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <nfd.h>

#include <thread>

namespace gui {

static nfdchar_t *pkg_path, *work_path;
static std::string state, title, zRIF;
static bool draw_file_dialog = true;
static bool delete_pkg_file, delete_work_file;

void draw_pkg_install_dialog(GuiState &gui, HostState &host) {
    nfdresult_t result = NFD_CANCEL;
    static float progress;

    static const auto BUTTON_SIZE = ImVec2(120.f, 45.f);

    if (draw_file_dialog) {
        result = NFD_OpenDialog("pkg", nullptr, &pkg_path);
        draw_file_dialog = false;
        if (result == NFD_OKAY)
            ImGui::OpenPopup("install");
        else if (result == NFD_CANCEL) {
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        } else {
            LOG_ERROR("Error initializing file dialog: {}", NFD_GetError());
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        }
    }

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("install", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (ImGui::CalcTextSize(title.c_str()).x / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (state.empty()) {
            title = "Select key type";
            if (ImGui::Button("Select Work.bin", BUTTON_SIZE))
                state = "work";
            if (ImGui::Button("Enter zRIF", BUTTON_SIZE))
                state = "zrif";
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("Cancel", BUTTON_SIZE)) {
                gui.file_menu.pkg_install_dialog = false;
                draw_file_dialog = true;
            }
        } else if (state == "work") {
            result = NFD_OpenDialog("bin", nullptr, &work_path);
            if (result == NFD_OKAY) {
                fs::ifstream binfile(string_utils::utf_to_wide(std::string(work_path)), std::ios::in | std::ios::binary | std::ios::ate);
                zRIF = rif2zrif(binfile);
                state = "install";
            } else
                state.clear();
        } else if (state == "zrif") {
            title = "Enter zRIF key";
            ImGui::PushItemWidth(640.f);
            ImGui::InputTextWithHint("##enter_zrif", "Please input your zRIF here", &zRIF);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Ctrl(Cmd) + C for copy, Ctrl(Cmd) + V to paste.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (BUTTON_SIZE.x + 10.f));
            if (ImGui::Button("Cancel", BUTTON_SIZE)) {
                state.clear();
                zRIF.clear();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) + 10.f);
            if (ImGui::Button("Ok", BUTTON_SIZE) && !zRIF.empty())
                state = "install";
        } else if (state == "install") {
            std::thread installation(([&host]() {
                if (install_pkg(std::string(pkg_path), host, zRIF, &progress))
                    state = "success";
                else
                    state = "fail";
                zRIF.clear();
            }));
            installation.detach();
            state = "installing";
        } else if (state == "success") {
            title = "PKG successfully installed";
            ImGui::TextColored(GUI_COLOR_TEXT, "%s [%s]", host.app_title_id.c_str(), host.app_title.c_str());
            if (host.app_category == std::string("gd"))
                ImGui::TextColored(GUI_COLOR_TEXT, "App version: %s", host.app_version.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Checkbox("Delete the pkg file?", &delete_pkg_file);
            if (work_path)
                ImGui::Checkbox("Delete the work.bin file?", &delete_work_file);
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f));
            if (ImGui::Button("OK", BUTTON_SIZE)) {
                if (delete_pkg_file) {
                    fs::remove(fs::path(string_utils::utf_to_wide(std::string(pkg_path))));
                    delete_pkg_file = false;
                }
                if (delete_work_file) {
                    fs::remove(fs::path(string_utils::utf_to_wide(std::string(work_path))));
                    delete_work_file = false;
                }
                update_notice_info(gui, host, "content");
                if (host.app_category == "gd")
                    refresh_app_list(gui, host);
                pkg_path = nullptr;
                work_path = nullptr;
                gui.file_menu.pkg_install_dialog = false;
                draw_file_dialog = true;
                state.clear();
            }
        } else if (state == "fail") {
            title = "Failed to install the pkg";
            ImGui::TextColored(GUI_COLOR_TEXT, "Please check log for more details.");
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f));
            if (ImGui::Button("OK", BUTTON_SIZE)) {
                refresh_app_list(gui, host);
                gui.file_menu.pkg_install_dialog = false;
                pkg_path = nullptr;
                draw_file_dialog = true;
                work_path = nullptr;
                state.clear();
            }
        } else if (state == "installing") {
            title = "Installing";
            ImGui::TextColored(GUI_COLOR_TEXT, "Installation in progress, please wait...");
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::SetCursorPosX((ImGui::GetWindowContentRegionWidth() / 2) - (150 / 2) + 10);
            ImGui::ProgressBar(progress / 100.f, ImVec2(150.f, 20.f), nullptr);
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }
}
} // namespace gui
