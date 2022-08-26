// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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
#include <gui/functions.h>
#include <host/dialog/filesystem.hpp>
#include <packages/functions.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <thread>

namespace gui {

std::string fw_version;
bool delete_pup_file;
std::filesystem::path pup_path = "";

static void get_firmware_version(EmuEnvState &emuenv) {
    fs::ifstream versionFile(emuenv.pref_path + L"/PUP_DEC/PUP/version.txt");

    if (versionFile.is_open()) {
        std::getline(versionFile, fw_version);
        versionFile.close();
    } else
        LOG_WARN("Firmware Version file not found!");

    fs::remove_all(fs::path(emuenv.pref_path) / "PUP_DEC");
}

void draw_firmware_install_dialog(GuiState &gui, EmuEnvState &emuenv) {
    auto lang = gui.lang.install_dialog;
    host::dialog::filesystem::Result result = host::dialog::filesystem::Result::CANCEL;

    static std::mutex install_mutex;
    static bool draw_file_dialog = true;
    static bool finished_installing = false;
    static std::atomic<uint32_t> progress(0);
    static const auto progress_callback = [&](uint32_t updated_progress) {
        progress = updated_progress;
    };
    std::lock_guard<std::mutex> lock(install_mutex);

    if (draw_file_dialog) {
        result = host::dialog::filesystem::open_file(pup_path, { { "PlayStation Update Package", { "PUP" } } });
        draw_file_dialog = false;
        finished_installing = false;

        if (result == host::dialog::filesystem::Result::SUCCESS) {
            std::thread installation([&emuenv]() {
                install_pup(emuenv.pref_path, pup_path.string(), progress_callback);
                std::lock_guard<std::mutex> lock(install_mutex);
                finished_installing = true;
                get_firmware_version(emuenv);
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
        ImGui::OpenPopup("Firmware Installation");
        if (ImGui::BeginPopupModal("Firmware Installation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["fw_installing"].c_str());
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::SetCursorPosX((ImGui::GetWindowContentRegionWidth() / 2) - (150 / 2) + 10);
            ImGui::ProgressBar(progress / 100.f, ImVec2(150.f, 20.f), nullptr);
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    } else {
        static const auto BUTTON_SIZE = ImVec2(60.f, 0.f);

        ImGui::OpenPopup("Firmware Installation");
        if (ImGui::BeginPopupModal("Firmware Installation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["successed_install_fw"].c_str());
            if (!fw_version.empty())
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", lang["fw_version"].c_str(), fw_version.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            const auto fw_font_package{ fs::path(emuenv.pref_path) / "sa0" };
            if (!fs::exists(fw_font_package) || fs::is_empty(fw_font_package)) {
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["no_font_exist"].c_str());
                if (ImGui::Button(lang["download_firmware_font_package"].c_str()))
                    open_path("https://bit.ly/2P2rb0r");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", lang["firmware_font_package_note"].c_str());
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
            ImGui::Checkbox(lang["delete_fw"].c_str(), &delete_pup_file);
            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
            if (ImGui::Button("OK", BUTTON_SIZE)) {
                if (delete_pup_file) {
                    fs::remove(fs::path(pup_path.wstring()));
                    delete_pup_file = false;
                }
                if (emuenv.cfg.initial_setup)
                    init_theme(gui, emuenv, gui.users[emuenv.cfg.user_id].theme_id);
                fw_version.clear();
                pup_path = nullptr;
                gui.file_menu.firmware_install_dialog = false;
                draw_file_dialog = true;
            }
        }
        ImGui::EndPopup();
    }
}
} // namespace gui
