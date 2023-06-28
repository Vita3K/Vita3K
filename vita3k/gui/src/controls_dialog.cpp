// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <SDL_scancode.h>
#include <config/functions.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <gui/functions.h>
#include <interface.h>

namespace gui {

static char const *SDL_key_to_string[]{ "[unset]", "[unknown]", "[unknown]", "[unknown]", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "Return/Enter", "Escape", "Backspace", "Tab", "Space", "-", "=", "[", "]", "\\", "NonUS #", ";", "'", "Grave", ",", ".", "/", "CapsLock", "F1",
    "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "PrtScrn", "ScrlLock", "Pause", "Insert", "Home", "PgUp", "Delete",
    "End", "PgDown", "Ar Right", "Ar Left", "Ar Down", "Ar Up", "NumLock/Clear", "Keypad /", "Keypad *", "Keypad -", "Keypad +",
    "Keypad Enter", "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4", "Keypad 5", "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9", "Keypad 0",
    "Keypad .", "NonUs \\", "App", "Power", "Keypad =", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "Execute",
    "Help", "Menu", "Select", "Stop", "Again", "Undo", "Cut", "Copy", "Paste", "Find", "Mute", "VolUp", "VolDown", "[unset]", "[unset]", "[unset]",
    "Keypad ,", "Kp = As400", "International1", "International2", "International3", "International4", "International5", "International6",
    "International7", "International8", "International9", "Lang1", "Lang2", "Lang3", "Lang4", "Lang5", "Lang6", "Lang7", "Lang8", "Lang9", "Alt Erase",
    "SysReq", "Cancel", "Clear", "Prior", "Return2", "Separator", "Out", "Oper", "ClearAgain", "Crsel", "Exsel", "[unset]", "[unset]", "[unset]",
    "[unset]", "[unset]", "[unset]", "[unset]", "[unset]", "[unset]", "[unset]", "[unset]", "Keypad 00", "Keypad 000", "ThousSeparat", "DecSeparat",
    "CurrencyUnit", "CurrencySubUnit", "Keypad (", "Keypad )", "Keypad {", "Keypad }", "Keypad Tab", "Keypad Backspace", "Keypad A", "Keypad B",
    "Keypad C", "Keypad D", "Keypad E", "Keypad F", "Keypad XOR", "Keypad Power", "Keypad %", "Keypad <", "Keypad >", "Keypad &", "Keypad &&",
    "Keypad |", "Keypad ||", "Keypad :", "Keypad #", "Keypad Space", "Keypad @", "Keypad !", "Keypad MemStr", "Keypad MemRec", "Keypad MemClr",
    "Keypad Mem+", "Keypad Mem-", "Keypad Mem*", "Keypad Mem/", "Keypad +/-", "Keypad Clear", "Keypad ClearEntry", "Keypad Binary", "Keypad Octal",
    "Keypad Dec", "Keypad HexaDec", "[unset]", "[unset]", "LCtrl", "LShift", "LAlt", "Win/Cmd", "RCtrl", "RShift", "RAlt", "RWin/Cmd" };

static const short total_key_entries = 28;

static bool exists_in_array(int *ptr, int val, size_t size) {
    if (!ptr || !size) {
        return false;
    }
    int *end = ptr + size;
    return std::find(ptr, end, val) != end;
}

static void prepare_map_array(EmuEnvState &emuenv, std::array<int, total_key_entries> &map) {
    map[0] = emuenv.cfg.keyboard_leftstick_up;
    map[1] = emuenv.cfg.keyboard_leftstick_down;
    map[2] = emuenv.cfg.keyboard_leftstick_right;
    map[3] = emuenv.cfg.keyboard_leftstick_left;
    map[4] = emuenv.cfg.keyboard_rightstick_up;
    map[5] = emuenv.cfg.keyboard_rightstick_down;
    map[6] = emuenv.cfg.keyboard_rightstick_right;
    map[7] = emuenv.cfg.keyboard_rightstick_left;
    map[8] = emuenv.cfg.keyboard_button_up;
    map[9] = emuenv.cfg.keyboard_button_down;
    map[10] = emuenv.cfg.keyboard_button_right;
    map[11] = emuenv.cfg.keyboard_button_left;
    map[12] = emuenv.cfg.keyboard_button_square;
    map[13] = emuenv.cfg.keyboard_button_cross;
    map[14] = emuenv.cfg.keyboard_button_circle;
    map[15] = emuenv.cfg.keyboard_button_triangle;
    map[16] = emuenv.cfg.keyboard_button_start;
    map[17] = emuenv.cfg.keyboard_button_select;
    map[18] = emuenv.cfg.keyboard_button_psbutton;
    map[19] = emuenv.cfg.keyboard_button_l1;
    map[20] = emuenv.cfg.keyboard_button_r1;
    map[21] = emuenv.cfg.keyboard_button_l2;
    map[22] = emuenv.cfg.keyboard_button_r2;
    map[23] = emuenv.cfg.keyboard_button_l3;
    map[24] = emuenv.cfg.keyboard_button_r3;
    map[25] = emuenv.cfg.keyboard_gui_toggle_gui;
    map[26] = emuenv.cfg.keyboard_gui_fullscreen;
    map[27] = emuenv.cfg.keyboard_gui_toggle_touch;
    map[28] = emuenv.cfg.keyboard_button_psbutton_shell;
}

bool need_open_error_duplicate_key_popup = false;

static void remapper_button(GuiState &gui, EmuEnvState &emuenv, int *button, const char *button_name, const char *tooltip = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", button_name);
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    ImGui::TableSetColumnIndex(1);
    // the association of the key
    int key_association = *button;
    if (ImGui::Button(SDL_key_to_string[key_association])) {
        gui.old_captured_key = key_association;
        gui.is_capturing_keys = true;
        // capture the original state
        std::array<int, total_key_entries> original_state;
        prepare_map_array(emuenv, original_state);
        while (gui.is_capturing_keys) {
            handle_events(emuenv, gui);
            *button = gui.captured_key;
            if (*button < 0 || *button > 231)
                *button = 0;
            else if (gui.is_key_capture_dropped || (!gui.is_capturing_keys && *button != key_association && exists_in_array(original_state.data(), *button, original_state.size()))) {
                // undo the changes
                *button = key_association;
                gui.is_key_capture_dropped = false;
                need_open_error_duplicate_key_popup = true;
            }
        }
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
}

void draw_controls_dialog(GuiState &gui, EmuEnvState &emuenv) {
    auto &lang = gui.lang.controls;
    float height = emuenv.viewport_size.y / emuenv.dpi_scale;
    if (ImGui::BeginMainMenuBar()) {
        height = height - ImGui::GetWindowHeight() * 2;
        ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowSize(ImVec2(0, height));
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin(lang["title"].c_str(), &gui.controls_menu.controls_dialog);
    if (ImGui::BeginTable("main", 2)) {
        ImGui::TableSetupColumn("button");
        ImGui::TableSetupColumn("mapped_button");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["button"].c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["mapped_button"].c_str());

        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_leftstick_up, lang["left_stick_up"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_leftstick_down, lang["left_stick_down"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_leftstick_right, lang["left_stick_right"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_leftstick_left, lang["left_stick_left"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_rightstick_up, lang["right_stick_up"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_rightstick_down, lang["right_stick_down"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_rightstick_right, lang["right_stick_right"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_rightstick_left, lang["right_stick_left"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_up, lang["d_pad_up"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_down, lang["d_pad_down"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_right, lang["d_pad_right"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_left, lang["d_pad_left"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_square, lang["square_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_cross, lang["cross_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_circle, lang["circle_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_triangle, lang["triangle_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_start, lang["start_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_select, lang["select_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_psbutton, lang["ps_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_l3, (lang["ps_button"] + " PS Vita OS").c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_l1, lang["l1_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_r1, lang["r1_button"].c_str());
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["ps_tv_mode"].c_str());
    ImGui::Spacing();
    if (ImGui::BeginTable("PSTV_mode", 2)) {
        ImGui::TableSetupColumn("button");
        ImGui::TableSetupColumn("mapped_button");
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_l2, lang["l2_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_r2, lang["r2_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_l3, lang["l3_button"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_button_r3, lang["r3_button"].c_str());
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["gui"].c_str());
    if (ImGui::BeginTable("gui", 2)) {
        ImGui::TableSetupColumn("button");
        ImGui::TableSetupColumn("mapped_button");
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_gui_fullscreen, lang["full_screen"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_gui_toggle_touch, lang["toggle_touch"].c_str(), lang["toggle_touch_note"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_gui_toggle_gui, lang["toggle_gui_visibility"].c_str(), lang["toggle_gui_visibility_note"].c_str());
        ImGui::EndTable();
    }
    if (need_open_error_duplicate_key_popup) {
        ImGui::OpenPopup(gui.lang.controls["error"].c_str());
        need_open_error_duplicate_key_popup = false;
    }
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(lang["error"].c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", lang["error_duplicate_key"].c_str());
        ImGui::NewLine();
        static const auto BUTTON_SIZE = ImVec2(120.f * emuenv.dpi_scale, 0.f);
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (BUTTON_SIZE.x / 2.f));
        if (ImGui::Button(emuenv.common_dialog.lang.common["ok"].c_str(), BUTTON_SIZE)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace gui
