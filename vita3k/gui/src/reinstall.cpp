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
#include <gui/functions.h>

#include <imgui.h>

namespace gui {

void draw_reinstall_dialog(GenericDialogState *status, HostState &host) {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("Reinstall this content?");
    ImGui::Text("This content is already installed.");
    ImGui::Spacing();
    ImGui::Text("Title: %s.\nTitle ID: %s.\nVersion: %s.", host.app_title.c_str(), host.app_title_id.c_str(), host.app_version.c_str());
    ImGui::Spacing();
    ImGui::Text("Do you want to reinstall it and overwrite existing data?");
    if (ImGui::Button("Yes")) {
        *status = CONFIRM_STATE;
    }
    ImGui::SameLine();
    if (ImGui::Button("No")) {
        *status = CANCEL_STATE;
    }
    ImGui::End();
}

} // namespace gui
