// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <dialog/state.h>
#include <host/dialog/filesystem.h>
#include <misc/cpp/imgui_stdlib.h>
#include <packages/license.h>

namespace gui {

void draw_license_install_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const auto BUTTON_SIZE = ImVec2(180.f * SCALE.x, 45.f * SCALE.y);

    auto &lang = gui.lang.install_dialog.license_install;
    auto &license = gui.lang.install_dialog.pkg_install;
    auto &indicator = gui.lang.indicator;
    auto &common = emuenv.common_dialog.lang.common;

    enum class State {
        UNDEFINED,
        LICENSE,
        ZRIF,
        SUCCESS,
        FAIL
    };

    static State state = State::UNDEFINED;

    static std::string title, zRIF;
    static std::filesystem::path license_path = "";
    static bool delete_license_file;

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size);
    ImGui::Begin("##license_install", &gui.file_menu.license_install_dialog, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##license_install_child", ImVec2(616.f * SCALE.x, 264.f * SCALE.y), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const auto POS_BUTTON = (ImGui::GetWindowWidth() / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, title.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    switch (state) {
    case State::UNDEFINED: {
        ImGui::SetCursorPosX(POS_BUTTON);
        title = license["select_license_type"];
        if (ImGui::Button(license["select_bin_rif"].c_str(), BUTTON_SIZE))
            state = State::LICENSE;
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button(license["enter_zrif"].c_str(), BUTTON_SIZE))
            state = State::ZRIF;
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(POS_BUTTON);
        if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE))
            gui.file_menu.license_install_dialog = false;
        break;
    }
    case State::LICENSE: {
        host::dialog::filesystem::Result result = host::dialog::filesystem::Result::CANCEL;
        result = host::dialog::filesystem::open_file(license_path, { { "PlayStation Vita software license file", { "bin", "rif" } } });
        if (result == host::dialog::filesystem::Result::SUCCESS) {
            if (copy_license(emuenv, fs::path(license_path.native())))
                state = State::SUCCESS;
            else
                state = State::FAIL;
        } else {
            if (result == host::dialog::filesystem::Result::ERROR)
                LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());

            state = State::UNDEFINED;
        }
        break;
    }
    case State::ZRIF: {
        title = license["enter_zrif_key"];
        ImGui::PushItemWidth(640.f * SCALE.x);
        ImGui::InputTextWithHint("##enter_zrif", license["input_zrif"].c_str(), &zRIF);
        ImGui::PopItemWidth();
        SetTooltipEx(license["copy_paste_zrif"].c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPos(ImVec2(POS_BUTTON - (BUTTON_SIZE.x / 2) - 10.f * emuenv.dpi_scale, ImGui::GetWindowSize().y / 2));
        if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE)) {
            state = State::UNDEFINED;
            zRIF.clear();
        }
        ImGui::SameLine(0, 20.f * SCALE.x);
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) && !zRIF.empty()) {
            if (create_license(emuenv, zRIF))
                state = State::SUCCESS;
            else
                state = State::FAIL;
        }
        break;
    }
    case State::SUCCESS: {
        title = indicator["install_complete"];
        ImGui::Spacing();
        ImGui::TextColored(GUI_COLOR_TEXT, "%s\n%s: %s\n%s: %s", lang["successed_install_license"].c_str(), gui.lang.settings.theme_background.theme.information["content_id"].c_str(), emuenv.license_content_id.c_str(), gui.lang.app_context.info["title_id"].c_str(), emuenv.license_title_id.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (license_path != "")
            ImGui::Checkbox(license["delete_bin_rif"].c_str(), &delete_license_file);
        ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
            if (delete_license_file) {
                fs::remove(fs::path(license_path.native()));
                delete_license_file = false;
            }
            license_path = "";
            gui.file_menu.license_install_dialog = false;
            state = State::UNDEFINED;
        }
        break;
    }
    case State::FAIL: {
        title = indicator["install_failed"];
        ImGui::SetCursorPosY(ImGui::GetWindowSize().y / 2.f - 20.f);
        TextColoredCentered(GUI_COLOR_TEXT, lang["failed_install_license"].c_str());
        ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
            gui.file_menu.license_install_dialog = false;
            license_path = "";
            state = State::UNDEFINED;
        }
    }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}
} // namespace gui
