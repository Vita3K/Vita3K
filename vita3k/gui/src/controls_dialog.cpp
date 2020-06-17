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

#include <config/functions.h>
#include <gui/functions.h>
#include <host/state.h>
#include <interface.h>

namespace gui {

static char const *SDL_key_to_string[]{ "[unset]", "[unknown]", "[unknown]", "[unknown]", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    "Return", "Escape", "Backspace", "Tab", "Space", "-", "=", "[", "]", "\\", "NonUS #", ";", "'", "Grave", ",", ".", "/", "CapsLock", "F1",
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

static void remapper_button(GuiState &gui, HostState &host, int *button, const char *button_name, const ImVec2 &dummy_size) {
    ImGui::Text("%-16s", button_name);
    ImGui::SameLine();
    ImGui::Dummy(dummy_size);
    ImGui::SameLine();
    if (ImGui::Button(SDL_key_to_string[*button])) {
        gui.old_captured_key = *button;
        gui.is_capturing_keys = true;
        while (gui.is_capturing_keys) {
            handle_events(host, gui);
            *button = gui.captured_key;
            if (*button < 0 || *button > 231)
                *button = 0;
        }
        config::serialize_config(host.cfg, host.cfg.config_path);
    }
}

void draw_controls_dialog(GuiState &gui, HostState &host) {
    float width = ImGui::GetWindowWidth() / 1.35f;
    float height = ImGui::GetWindowHeight() / 1.25f;
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("Controls", &gui.controls_menu.controls_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%-16s    %-16s", "Button", "Mapped button");
    remapper_button(gui, host, &host.cfg.keyboard_leftstick_up, "Left stick up", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_leftstick_down, "Left stick down", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_leftstick_right, "Left stick right", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_leftstick_left, "Left stick left", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_rightstick_up, "Right stick up", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_rightstick_down, "Right stick down", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_rightstick_right, "Right stick right", ImVec2(0.3f, 0.3f));
    remapper_button(gui, host, &host.cfg.keyboard_rightstick_left, "Right stick left", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_up, "D-pad up", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_down, "D-pad down", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_right, "D-pad right", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_left, "D-pad left", ImVec2(7.0f, 7.0f));
    if (host.cfg.pstv_mode) {
        remapper_button(gui, host, &host.cfg.keyboard_button_l1, "L1 button", ImVec2(7.0f, 7.0f));
        remapper_button(gui, host, &host.cfg.keyboard_button_r1, "R1 button", ImVec2(7.0f, 7.0f));
        remapper_button(gui, host, &host.cfg.keyboard_button_l2, "L2 button", ImVec2(7.0f, 7.0f));
        remapper_button(gui, host, &host.cfg.keyboard_button_r2, "R2 button", ImVec2(7.0f, 7.0f));
        remapper_button(gui, host, &host.cfg.keyboard_button_l3, "L3 button", ImVec2(7.0f, 7.0f));
        remapper_button(gui, host, &host.cfg.keyboard_button_r3, "R3 button", ImVec2(7.0f, 7.0f));
    } else {
        remapper_button(gui, host, &host.cfg.keyboard_button_l1, "L button", ImVec2(7.0f, 7.0f));
        remapper_button(gui, host, &host.cfg.keyboard_button_r1, "R button", ImVec2(7.0f, 7.0f));
    }
    remapper_button(gui, host, &host.cfg.keyboard_button_square, "Square button", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_cross, "Cross button", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_circle, "Circle button", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_triangle, "Triangle button", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_start, "Start button", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_select, "Select button", ImVec2(7.0f, 7.0f));
    remapper_button(gui, host, &host.cfg.keyboard_button_psbutton, "PS Button", ImVec2(7.0f, 7.0f));
    ImGui::Separator();
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "%-16s", "GUI");
    ImGui::Text("%-16s    %-16s", "Toggle Touch", "T");
    ImGui::Text("%-16s    %-16s", "Toggle GUI visibility", "G");

    ImGui::End();
}

} // namespace gui
