// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <host/functions.h>
#include <misc/cpp/imgui_stdlib.h>
#include <util/string_utils.h>

#include <nfd.h>

namespace gui {

static std::string state, title, zRIF;
nfdchar_t *work_path;

void draw_license_install_dialog(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto BUTTON_SIZE = ImVec2(160.f * SCALE.x, 45.f * SCALE.y);

    auto indicator = gui.lang.indicator;
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size);
    ImGui::Begin("License Install", &gui.file_menu.license_install_dialog, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::BeginChild("License Install", ImVec2(540.f * SCALE.x, 220.f * SCALE.y), true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const auto POS_BUTTON = (ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (ImGui::CalcTextSize(title.c_str()).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (state.empty()) {
        ImGui::SetCursorPosX(POS_BUTTON);
        title = "Select license type";
        if (ImGui::Button("Select Work.bin/rif", BUTTON_SIZE))
            state = "work";
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button("Enter zRIF", BUTTON_SIZE))
            state = "zrif";
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button("Cancel", BUTTON_SIZE))
            gui.file_menu.license_install_dialog = false;
    } else if (state == "work") {
        nfdresult_t result = NFD_CANCEL;
        result = NFD_OpenDialog("bin,rif", nullptr, &work_path);
        if (result == NFD_OKAY) {
            if (copy_license(host, work_path))
                state = "success";
            else
                state = "fail";
        } else
            state.clear();
    } else if (state == "zrif") {
        title = "Enter zRIF key";
        ImGui::PushItemWidth(540.f * SCALE.x);
        ImGui::InputTextWithHint("##enter_zrif", "Please input your zRIF here", &zRIF);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ctrl(Cmd) + C for copy, Ctrl(Cmd) + V to paste.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPos(ImVec2(POS_BUTTON - (BUTTON_SIZE.x / 2) - 10.f, ImGui::GetWindowSize().y / 2));
        if (ImGui::Button("Cancel", BUTTON_SIZE)) {
            state.clear();
            zRIF.clear();
        }
        ImGui::SameLine(0, 20.f);
        if (ImGui::Button("Ok", BUTTON_SIZE) && !zRIF.empty()) {
            if (create_license(host, zRIF))
                state = "success";
            else
                state = "fail";
        }
    } else if (state == "success") {
        title = !indicator["install_complete"].empty() ? indicator["install_complete"] : "Installation complete.";
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "Succes install license.\nContent ID: %s\nTitle ID: %s", host.license_content_id.c_str(), host.license_title_id.c_str());
        ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - 20.f));
        if (ImGui::Button("OK", BUTTON_SIZE)) {
            work_path = nullptr;
            gui.file_menu.license_install_dialog = false;
            state.clear();
        }
    } else if (state == "fail") {
        title = !indicator["install_failed"].empty() ? gui.lang.indicator["install_failed"].c_str() : "Could not install.";
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowSize().x / 2.f) - (ImGui ::CalcTextSize("Please check log for more details.").x / 2.f), ImGui::GetWindowSize().y / 2.f - 20.f));
        ImGui::TextColored(GUI_COLOR_TEXT, "Please check log for more details.");
        ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
        if (ImGui::Button("OK", BUTTON_SIZE)) {
            gui.file_menu.license_install_dialog = false;
            work_path = nullptr;
            state.clear();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
} // namespace gui
