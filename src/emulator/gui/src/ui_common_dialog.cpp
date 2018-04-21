// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <gui/functions.h>
#include <imgui.h>

#include <host/state.h>
#include <kernel/thread_functions.h>
#include <kernel/thread_state.h>
#include <util/resource.h>
#include <util/string_convert.h>

void DrawImeDialog(HostState &host) {
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin(host.gui.common_dialog.ime.title.c_str());
    if (host.gui.common_dialog.ime.multiline) {
        ImGui::InputTextMultiline(
            "",
            host.gui.common_dialog.ime.text,
            host.gui.common_dialog.ime.max_length);
    } else {
        ImGui::InputText(
            "",
            host.gui.common_dialog.ime.text,
            host.gui.common_dialog.ime.max_length);
    }
    ImGui::SameLine();
    if (ImGui::Button("Submit")) {
        host.gui.common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
        host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        std::string result = host.gui.common_dialog.ime.text;
        std::u16string result16 = utf8_to_utf16(result);
        memcpy(host.gui.common_dialog.ime.result, result16.c_str(), result16.size() * sizeof(uint16_t));
    }
    if (host.gui.common_dialog.ime.cancelable) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            host.gui.common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
            host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        }
    }
    ImGui::End();
}

void DrawCommonDialog(HostState &host) {
    if (host.gui.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
        switch (host.gui.common_dialog.type) {
        case IME_DIALOG:
            DrawImeDialog(host);
            break;
        default:
            break;
        }
    }
}
