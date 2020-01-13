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

#include "private.h"

#include <gui/functions.h>

#include <util/string_utils.h>

#include <SDL.h>

namespace gui {

static void draw_ime_dialog(DialogState &common_dialog) {
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin(common_dialog.ime.title.c_str());
    if (common_dialog.ime.multiline) {
        ImGui::InputTextMultiline(
            "",
            common_dialog.ime.text,
            common_dialog.ime.max_length);
    } else {
        ImGui::InputText(
            "",
            common_dialog.ime.text,
            common_dialog.ime.max_length);
    }
    ImGui::SameLine();
    if (ImGui::Button("Submit")) {
        common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
        common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        std::string result = common_dialog.ime.text;
        std::u16string result16 = string_utils::utf8_to_utf16(result);
        memcpy(common_dialog.ime.result, result16.c_str(), result16.size() * sizeof(uint16_t));
    }
    if (common_dialog.ime.cancelable) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
            common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
            common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
        }
    }
    ImGui::End();
}

static void draw_message_dialog(DialogState &common_dialog) {
    const ImVec2 PROGRESS_BAR_SIZE = ImVec2(120.f, 5.f);
    ImGui::SetNextWindowPosCenter();
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("Message Dialog");
    ImGui::Text("%s", common_dialog.msg.message.c_str());
    if (common_dialog.msg.has_progress_bar) {
        const char dummy_buf[32] = "";
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_PROGRESS_BAR_BG);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - PROGRESS_BAR_SIZE.x / 2);
        ImGui::ProgressBar(common_dialog.msg.bar_rate / 100.f, PROGRESS_BAR_SIZE, dummy_buf);
        ImGui::PopStyleColor(2);
        std::string progress = std::to_string(static_cast<int>(common_dialog.msg.bar_rate)).append("%");
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - ImGui::CalcTextSize(progress.c_str()).x / 2);
        ImGui::Text("%s", progress.c_str());
    }
    for (int i = 0; i < common_dialog.msg.btn_num; i++) {
        if (ImGui::Button(common_dialog.msg.btn[i].c_str())) {
            common_dialog.msg.status = common_dialog.msg.btn_val[i];
            common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
            common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        }
        ImGui::SameLine();
    }
    ImGui::End();
}

static void draw_trophy_setup_dialog(DialogState &common_dialog) {
    int timer = (static_cast<int64_t>(common_dialog.trophy.tick) - static_cast<int64_t>(SDL_GetTicks())) / 1000;
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
        common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    }
}

void draw_common_dialog(GuiState &gui, HostState &host) {
    if (host.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
        switch (host.common_dialog.type) {
        case IME_DIALOG:
            draw_ime_dialog(host.common_dialog);
            break;
        case MESSAGE_DIALOG:
            draw_message_dialog(host.common_dialog);
            break;
        case TROPHY_SETUP_DIALOG:
            draw_trophy_setup_dialog(host.common_dialog);
            break;
        default:
            break;
        }
    }
}

} // namespace gui
