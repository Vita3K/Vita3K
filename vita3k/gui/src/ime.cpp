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

#include <gui/imgui_impl_sdl.h>
#include <mem/functions.h>

namespace gui {

static const std::vector<std::string> shift_key = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
    "A", "S", "D", "F", "G", "H", "J", "K", "L",
    "Z", "X", "C", "V", "B", "N", "M"
};

static const std::vector<std::string> key = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p",
    "a", "s", "d", "f", "g", "h", "j", "k", "l",
    "z", "x", "c", "v", "b", "n", "m"
};

enum CapsLevel {
    NO = 0,
    YES = 1,
    LOCK = 2
};

const void update_str(Ime &ime, const std::string &key) {
    if (ime.str.empty())
        ime.edit_text.preeditIndex = 0;
    if (ime.str.size() < ime.param.maxTextLength) {
        ime.str.insert(ime.edit_text.caretIndex, key);
        ime.edit_text.editIndex = ime.edit_text.caretIndex;
        ime.edit_text.caretIndex++;
    }
    if (std::isspace(key[0])) {
        ime.caretIndex = ime.edit_text.caretIndex;
        ime.edit_text.preeditIndex = ime.edit_text.caretIndex;
        ime.edit_text.preeditLength = ime.edit_text.editLengthChange = 0;
    } else {
        ime.edit_text.editLengthChange == -1 ? ime.edit_text.editLengthChange = 1 : ++ime.edit_text.editLengthChange;
        ime.edit_text.preeditLength++;
    }
    if (ime.caps_level == YES)
        ime.caps_level = NO;
    ime.event_id = SCE_IME_EVENT_UPDATE_TEXT;
}

void draw_ime(HostState &host) {
    const auto displaysize = ImGui::GetIO().DisplaySize;
    const auto BUTTON_SIZE = ImVec2(90.f, 52.f);
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(displaysize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##ime", &host.ime.state, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(0.f, displaysize.y - 248.f), ImGuiCond_Always, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, GUI_SMOOTH_GRAY);
    ImGui::BeginChild("##ime_keyèboard", ImVec2(displaysize.x, 248.f), true, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::Columns(9, nullptr, false);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_BLACK);
    for (const auto &key : host.ime.caps_level == NO ? key : shift_key) {
        if (ImGui::Button(key.c_str(), BUTTON_SIZE) && (host.ime.str.size() < host.ime.param.maxTextLength))
            update_str(host.ime, key);
        ImGui::NextColumn();
    }
    ImGui::PopStyleColor(2);
    ImGui::Columns(1);
    ImGui::SameLine(0, 15);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_IME_BUTTON_BG);
    if (ImGui::Button("Back", BUTTON_SIZE) && (host.ime.edit_text.caretIndex > 0)) {
        host.ime.str.erase(host.ime.edit_text.caretIndex - 1, 1);
        host.ime.edit_text.editIndex = host.ime.edit_text.caretIndex;
        --host.ime.edit_text.caretIndex;
        if (host.ime.edit_text.preeditIndex > host.ime.edit_text.caretIndex)
            host.ime.edit_text.preeditIndex = host.ime.caretIndex = host.ime.edit_text.caretIndex;
        host.ime.edit_text.editLengthChange = -1;
        if (host.ime.edit_text.preeditLength > 0)
            --host.ime.edit_text.preeditLength;
        host.ime.event_id = SCE_IME_EVENT_UPDATE_TEXT;
        if (host.ime.str.empty() && (host.ime.caps_level == NO))
            host.ime.caps_level = YES;
    }
    ImGui::PopStyleColor();
    const auto BUTTON_HALF_SIZE = ImVec2(BUTTON_SIZE.x / 2.f, BUTTON_SIZE.y);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT_BLACK);
    if (ImGui::Button("V", BUTTON_HALF_SIZE))
        host.ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 14);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_IME_BUTTON_BG);
    if (ImGui::Button("Shift", BUTTON_SIZE)) {
        if (host.ime.edit_text.caretIndex == 0)
            host.ime.caps_level = host.ime.caps_level == YES ? NO : ++host.ime.caps_level;
        else
            host.ime.caps_level = host.ime.caps_level == LOCK ? NO : ++host.ime.caps_level;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 14);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_BLACK);
    if (ImGui::Button(",", BUTTON_HALF_SIZE))
        update_str(host.ime, ",");
    ImGui::PopStyleColor(2);
    ImGui::SameLine(0, 14);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_IME_BUTTON_BG);
    if (ImGui::Button("<", BUTTON_SIZE) && host.ime.edit_text.caretIndex > 0) {
        if (host.ime.caretIndex > 0)
            --host.ime.caretIndex;
        --host.ime.edit_text.caretIndex;
        if ((host.ime.edit_text.caretIndex == 0) && (host.ime.caps_level == NO))
            host.ime.caps_level = YES;
        host.ime.event_id = SCE_IME_EVENT_UPDATE_CARET;
    }
    ImGui::SameLine(0, 14);
    if (ImGui::Button("Space", ImVec2(BUTTON_SIZE.x * 3.4f, BUTTON_SIZE.y))) {
        update_str(host.ime, " ");
    }
    ImGui::SameLine(0, 14);
    if (ImGui::Button(">", BUTTON_SIZE)) {
        host.ime.edit_text.editIndex = host.ime.edit_text.caretIndex;
        if (host.ime.edit_text.caretIndex < host.ime.str.size()) {
            host.ime.edit_text.caretIndex++;
            host.ime.caretIndex++;
            host.ime.event_id = SCE_IME_EVENT_UPDATE_CARET;
        } else {
            host.ime.caretIndex = host.ime.edit_text.caretIndex;
            host.ime.edit_text.preeditIndex = host.ime.edit_text.caretIndex;
            host.ime.edit_text.editLengthChange = host.ime.edit_text.preeditLength = 0;
            host.ime.event_id = SCE_IME_EVENT_UPDATE_TEXT;
        }
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 14);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_BLACK);
    if (ImGui::Button(".", BUTTON_HALF_SIZE))
        update_str(host.ime, ".");
    ImGui::PopStyleColor(2);
    ImGui::SameLine(0, 14);
    ImGui::PushStyleColor(ImGuiCol_Button, GUI_PROGRESS_BAR);
    if (ImGui::Button(host.ime.enter_label.c_str(), BUTTON_SIZE))
        host.ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
