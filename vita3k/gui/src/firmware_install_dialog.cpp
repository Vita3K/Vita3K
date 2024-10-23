// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <config/state.h>
#include <dialog/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.h>
#include <packages/functions.h>
#include <util/log.h>

#include <thread>

namespace gui {

void draw_firmware_install_dialog(GuiState &gui, EmuEnvState &emuenv) {
    static std::string fw_version;
    static bool delete_pup_file;
    static fs::path pup_path{};

    static std::mutex install_mutex;
    static bool draw_file_dialog = true;
    static bool finished_installing = false;
    static std::atomic<uint32_t> progress(0);
    static const auto progress_callback = [&](uint32_t updated_progress) {
        progress = updated_progress;
    };
    std::lock_guard<std::mutex> lock(install_mutex);

    auto &lang = gui.lang.install_dialog.firmware_install;
    auto &common = emuenv.common_dialog.lang.common;

    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const ImVec2 WINDOW_SIZE(616.f * SCALE.x, 264.f * SCALE.y);
    const ImVec2 BUTTON_SIZE(160.f * SCALE.x, 45.f * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2), emuenv.logical_viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    if (draw_file_dialog) {
        auto result = host::dialog::filesystem::open_file(pup_path, { { "PlayStation Vita Firmware Package", { "PUP" } } });
        draw_file_dialog = false;
        finished_installing = false;

        if (result == host::dialog::filesystem::Result::SUCCESS) {
            std::thread installation([&emuenv]() {
                fw_version = install_pup(emuenv.pref_path, pup_path, progress_callback);
                std::lock_guard<std::mutex> lock(install_mutex);
                finished_installing = true;
            });
            installation.detach();
        } else if (result == host::dialog::filesystem::Result::CANCEL) {
            gui.file_menu.firmware_install_dialog = false;
            draw_file_dialog = true;
        } else {
            LOG_ERROR("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
            gui.file_menu.firmware_install_dialog = false;
            draw_file_dialog = true;
        }
    }

    if (!finished_installing) {
        ImGui::OpenPopup("firmware_installation");
        if (ImGui::BeginPopupModal("firmware_installation", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
            ImGui::SetWindowFontScale(RES_SCALE.x);
            TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang["firmware_installation"].c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30.f * SCALE.y);
            TextColoredCentered(GUI_COLOR_TEXT, lang["firmware_installing"].c_str());
            const float PROGRESS_BAR_WIDTH = 502.f * SCALE.x;
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (PROGRESS_BAR_WIDTH / 2.f), ImGui::GetCursorPosY() + 30.f * SCALE.y));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::ProgressBar(progress / 100.f, ImVec2(PROGRESS_BAR_WIDTH, 15.f * SCALE.x), "");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.f * SCALE.y);
            TextColoredCentered(GUI_COLOR_TEXT, std::to_string(progress).append("%").c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    } else {
        ImGui::OpenPopup("firmware_installation");
        if (ImGui::BeginPopupModal("firmware_installation", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration)) {
            ImGui::SetWindowFontScale(RES_SCALE.x);
            const auto POS_BUTTON = (WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f) + (10.f * SCALE.x);
            TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang["successed_install_firmware"].c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (!fw_version.empty())
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang["firmware_version"].c_str(), fw_version.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            const auto fw_font_package{ emuenv.pref_path / "sa0" };
            if (!fs::exists(fw_font_package) || fs::is_empty(fw_font_package)) {
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["no_font_exist"].c_str());
                if (ImGui::Button(gui.lang.welcome["download_firmware_font_package"].c_str()))
                    open_path("https://bit.ly/2P2rb0r");
                SetTooltipEx(lang["firmware_font_package_description"].c_str());
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
#ifndef __ANDROID__
            ImGui::Checkbox(lang["delete_firmware"].c_str(), &delete_pup_file);
            ImGui::Spacing();
#endif
            ImGui::SetCursorPos(ImVec2(POS_BUTTON, ImGui::GetWindowSize().y - BUTTON_SIZE.y - (20.f * SCALE.y)));
            if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
                if (delete_pup_file) {
                    fs::remove(pup_path);
                    delete_pup_file = false;
                }
                get_modules_list(gui, emuenv);
                if (emuenv.cfg.initial_setup)
                    init_theme(gui, emuenv, gui.users[emuenv.cfg.user_id].theme_id);
                else if (gui::init_bgm(emuenv, { "pd0", "data/systembgm/initialsetup.at9" }))
                    gui::switch_bgm_state(false);
                fw_version.clear();
                pup_path = "";
                gui.file_menu.firmware_install_dialog = false;
                draw_file_dialog = true;
            }
        }
        ImGui::EndPopup();
    }
}
} // namespace gui
