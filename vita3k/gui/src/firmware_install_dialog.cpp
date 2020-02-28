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
#include <host/functions.h>
#include <util/log.h>

#include <nfd.h>

namespace gui {

std::string fw_version;
bool delete_fw_file = false;
nfdchar_t *fw_path = nullptr;

static void get_firmware_version(HostState &host) {
    std::ifstream versionFile(host.pref_path + "/PUP_DEC/PUP/version.txt");

    if (versionFile.is_open()) {
        std::getline(versionFile, fw_version);
        versionFile.close();
    } else
        LOG_WARN("Firmware Version file not found!");

    fs::remove_all(fs::path(host.pref_path) / "PUP_DEC");
}

void draw_firmware_install_dialog(GuiState &gui, HostState &host) {
    nfdresult_t result = NFD_CANCEL;

    static bool draw_file_dialog = true;

    if (draw_file_dialog) {
        result = NFD_OpenDialog("PUP", nullptr, &fw_path);
        draw_file_dialog = false;

        if (result == NFD_OKAY) {
            install_pup(fw_path, host.pref_path);
            LOG_INFO("Extraction complete!");
            get_firmware_version(host);
        } else if (result == NFD_CANCEL) {
            gui.file_menu.firmware_install_dialog = false;
            draw_file_dialog = true;
        } else {
            LOG_ERROR("Error initializing file dialog: {}", NFD_GetError());
            gui.file_menu.firmware_install_dialog = false;
            draw_file_dialog = true;
        }
    }

    static const auto BUTTON_SIZE = ImVec2(60.f, 0.f);

    ImGui::OpenPopup("Firmware Installation");
    if (ImGui::BeginPopupModal("Firmware Installation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(GUI_COLOR_TEXT, "Firmware successfully installed.");
        if (!fw_version.empty())
            ImGui::TextColored(GUI_COLOR_TEXT, "Firmware version: %s", fw_version.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Checkbox("Delete the firmware installation file?", &delete_fw_file);
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
        if (ImGui::Button("OK", BUTTON_SIZE)) {
            if (delete_fw_file) {
                fs::remove(fs::path(fw_path));
                delete_fw_file = false;
            }
            fw_version.clear();
            fw_path = nullptr;
            get_modules_list(gui, host);
            gui.file_menu.firmware_install_dialog = false;
            draw_file_dialog = true;
        }
    }
    ImGui::EndPopup();
}
} // namespace gui
