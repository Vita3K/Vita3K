// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <host/dialog/filesystem.hpp>
#include <misc/cpp/imgui_stdlib.h>
#include <packages/functions.h>
#include <util/string_utils.h>

namespace gui {

static std::string state, title, zRIF;
std::filesystem::path work_path = "";
static bool delete_work_file;

void draw_license_install_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const auto BUTTON_SIZE = ImVec2(180.f * SCALE.x, 45.f * SCALE.y);

    auto lang = gui.lang.install_dialog.license_install;
    auto zrif = gui.lang.install_dialog.pkg_install;
    auto indicator = gui.lang.indicator;
    auto common = emuenv.common_dialog.lang.common;

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size);
    ImGui::Begin("##license_install", &gui.file_menu.license_install_dialog, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##license_install_child", ImVec2(616.f * SCALE.x, 264.f * SCALE.y), true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const auto POS_BUTTON = (ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (ImGui::CalcTextSize(title.c_str()).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (state.empty()) {
        ImGui::SetCursorPosX(POS_BUTTON);
        title = lang["select_license_type"];
        if (ImGui::Button(lang["select_bin_rif"].c_str(), BUTTON_SIZE))
            state = "work";
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button(zrif["enter_zrif"].c_str(), BUTTON_SIZE))
            state = "zrif";
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE))
            gui.file_menu.license_install_dialog = false;
    } else if (state == "work") {
        host::dialog::filesystem::Result result = host::dialog::filesystem::Result::CANCEL;
        result = host::dialog::filesystem::open_file(work_path, { { "PlayStation Vita software license file", { "bin", "rif" } } });
        if (result == host::dialog::filesystem::Result::SUCCESS) {
            if (copy_license(emuenv, fs::path(work_path.wstring())))
                state = "success";
            else
                state = "fail";
        } else
            state.clear();
    } else if (state == "zrif") {
        title = zrif["enter_zrif_key"];
        ImGui::PushItemWidth(640.f * SCALE.x);
        ImGui::InputTextWithHint("##enter_zrif", zrif["input_zrif"].c_str(), &zRIF);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", zrif["copy_paste_zrif"].c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPos(ImVec2(POS_BUTTON - (BUTTON_SIZE.x / 2) - 10.f * emuenv.dpi_scale, ImGui::GetWindowSize().y / 2));
        if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE)) {
            state.clear();
            zRIF.clear();
        }
        ImGui::SameLine(0, 20.f * SCALE.x);
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) && !zRIF.empty()) {
            if (create_license(emuenv, zRIF))
                state = "success";
            else
                state = "fail";
        }
    } else if (state == "success") {
        title = indicator["install_complete"];
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s\n%s: %s\n%s: %s", lang["successed_install_license"].c_str(), gui.lang.settings.theme_background.theme.information["content_id"].c_str(), emuenv.license_content_id.c_str(), gui.lang.app_context["title_id"].c_str(), emuenv.license_title_id.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (work_path != "")
            ImGui::Checkbox(lang["delete_bin_rif"].c_str(), &delete_work_file);
        ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
            if (delete_work_file) {
                fs::remove(fs::path(work_path.wstring()));
                delete_work_file = false;
            }
            work_path = nullptr;
            gui.file_menu.license_install_dialog = false;
            state.clear();
        }
    } else if (state == "fail") {
        title = indicator["install_failed"];
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowSize().x / 2.f) - (ImGui ::CalcTextSize(lang["check_bin_rif"].c_str()).x / 2.f), ImGui::GetWindowSize().y / 2.f - 20.f));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["check_bin_rif"].c_str());
        ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
            gui.file_menu.license_install_dialog = false;
            work_path = nullptr;
            state.clear();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}
} // namespace gui
