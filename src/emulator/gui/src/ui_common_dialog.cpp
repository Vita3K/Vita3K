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
#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>
#include <util/resource.h>
#include <util/string_utils.h>

#include <SDL.h>

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
        host.gui.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        std::string result = host.gui.common_dialog.ime.text;
        std::u16string result16 = string_utils::utf8_to_utf16(result);
        memcpy(host.gui.common_dialog.ime.result, result16.c_str(), result16.size() * sizeof(uint16_t));
    }
    if (host.gui.common_dialog.ime.cancelable) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            host.gui.common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
            host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
            host.gui.common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
        }
    }
    ImGui::End();
}

void DrawMessageDialog(HostState &host) {
    ImGui::SetNextWindowPosCenter();
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("Message Dialog");
    ImGui::Text("%s", host.gui.common_dialog.msg.message.c_str());
    for (int i = 0; i < host.gui.common_dialog.msg.btn_num; i++) {
        if (ImGui::Button(host.gui.common_dialog.msg.btn[i].c_str())) {
            host.gui.common_dialog.msg.status = host.gui.common_dialog.msg.btn_val[i];
            host.gui.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
            host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        }
        ImGui::SameLine();
    }
    ImGui::End();
}

void DrawTrophySetupDialog(HostState &host) {
    int timer = (static_cast<int64_t>(host.gui.common_dialog.trophy.tick) - static_cast<int64_t>(SDL_GetTicks())) / 1000;
    if (timer > 0) {
        ImGui::SetNextWindowPos(ImVec2(30, 30));
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("Trophy system ready");
        ImGui::Text("Trophy dialog setup correctly");
        std::string closeup_text = "This dialog will close automatically in ";
        closeup_text += std::to_string(timer);
        closeup_text += " seconds.";
        ImGui::Text("%s", closeup_text.c_str());
        ImGui::End();
    } else {
        host.gui.common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        host.gui.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    }
}

void DrawCommonDialog(HostState &host) {
    if (host.gui.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
        switch (host.gui.common_dialog.type) {
        case IME_DIALOG:
            DrawImeDialog(host);
            break;
        case MESSAGE_DIALOG:
            DrawMessageDialog(host);
            break;
        case TROPHY_SETUP_DIALOG:
            DrawTrophySetupDialog(host);
            break;
        default:
            break;
        }
    }
}
