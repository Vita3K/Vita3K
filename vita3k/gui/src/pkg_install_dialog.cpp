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
#include <host/sce_types.h>
#include <misc/cpp/imgui_stdlib.h>
#include <util/log.h>

#include <nfd.h>

namespace gui {

void draw_pkg_install_dialog(GuiState &gui, HostState &host) {
    nfdresult_t result = NFD_CANCEL;

    static nfdchar_t *pkg_path;
    static bool draw_file_dialog = true;
    bool is_entering_zrif = true;
    static bool delete_pkg_file = false;
    static const auto BUTTON_SIZE = ImVec2(60.f, 0.f);
    bool pkg_success = false;

    if (draw_file_dialog) {
        result = NFD_OpenDialog("pkg", nullptr, &pkg_path);
        draw_file_dialog = false;
        if (result == NFD_OKAY) {
            ImGui::OpenPopup("Enter zRIF");
        } else if (result == NFD_CANCEL) {
            gui.file_menu.pkg_install_dialog = false;
            is_entering_zrif = true;
            draw_file_dialog = true;
        } else {
            LOG_ERROR("Error initializing file dialog: {}", NFD_GetError());
            gui.file_menu.pkg_install_dialog = false;
            is_entering_zrif = true;
            draw_file_dialog = true;
        }
    }

    if (ImGui::BeginPopupModal("PKG Installation success", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(GUI_COLOR_TEXT, "PKG successfully installed.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Checkbox("Delete the pkg file?", &delete_pkg_file);
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 30);
        if (ImGui::Button("OK", BUTTON_SIZE)) {
            if (delete_pkg_file) {
                fs::remove(fs::path(pkg_path));
                delete_pkg_file = false;
            }
            pkg_path = nullptr;
            is_entering_zrif = true;
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("PKG Installation failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(GUI_COLOR_TEXT, "Failed to install the pkg. \nPlease check log for more details.");
        if (ImGui::Button("OK", BUTTON_SIZE)) {
            is_entering_zrif = true;
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Enter zRIF", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputTextWithHint("##enter_zrif", "Please input your zRIF here", &host.zRIF);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ctrl(Cmd) + C for copy, Ctrl(Cmd) + V to paste.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Confirm") && !host.zRIF.empty()) {
            if (install_pkg(pkg_path, host.pref_path, host)) {
                pkg_success = true;
				is_entering_zrif = false;
            } else {
                pkg_success = false;
                is_entering_zrif = false;
            }
        }
        if (ImGui::Button("Cancel")) {
            is_entering_zrif = true;
            ImGui::CloseCurrentPopup();
            gui.file_menu.pkg_install_dialog = false;
            draw_file_dialog = true;
        }
        ImGui::EndPopup();
    }
    if (is_entering_zrif == false) {
		if (pkg_success) {
		    ImGui::OpenPopup("PKG Installation success");
			refresh_game_list(gui, host);
		}
		else {
			ImGui::OpenPopup("PKG Installation failed");
			refresh_game_list(gui, host);
		}
	}
}
} // namespace gui