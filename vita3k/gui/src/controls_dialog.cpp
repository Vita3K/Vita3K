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
#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <config/functions.h>
#include <gui/functions.h>
#include <host/state.h>
#include <interface.h>
#include <iostream>

static char const *SDL_key_to_string[]{
    //Starts at 0, ends at 231
    "[unset]",
    "[unknown]",
    "[unknown]",
    "[unknown]",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "0",
    "Return",
    "Escape",
    "Backspace",
    "Tab",
    "Space",
    "-",
    "=",
    "[",
    "]",
    "\\",
    "NonUS #",
    ";",
    "'",
    "Grave",
    ",",
    ".",
    "/",
    "CapsLock",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "PrtScrn",
    "ScrlLock",
    "Pause",
    "Insert",
    "Home",
    "PgUp",
    "Delete",
    "End",
    "PgDown",
    "Ar Right",
    "Ar Left",
    "Ar Down",
    "Ar Up",
    "NumLock/Clear",
    "Keypad /",
    "Keypad *",
    "Keypad -",
    "Keypad +",
    "Keypad Enter",
    "Keypad 1",
    "Keypad 2",
    "Keypad 3",
    "Keypad 4",
    "Keypad 5",
    "Keypad 6",
    "Keypad 7",
    "Keypad 8",
    "Keypad 9",
    "Keypad 0",
    "Keypad .",
    "NonUs \\",
    "App",
    "Power",
    "Keypad =",
    "F13",
    "F14",
    "F15",
    "F16",
    "F17",
    "F18",
    "F19",
    "F20",
    "F21",
    "F22",
    "F23",
    "F24",
    "Execute",
    "Help",
    "Menu",
    "Select",
    "Stop",
    "Again",
    "Undo",
    "Cut",
    "Copy",
    "Paste",
    "Find",
    "Mute",
    "VolUp",
    "VolDown",
    "[unset]",
    "[unset]",
    "[unset]",
    "Keypad ,",
    "Kp = As400",
    "International1",
    "International2",
    "International3",
    "International4",
    "International5",
    "International6",
    "International7",
    "International8",
    "International9",
    "Lang1",
    "Lang2",
    "Lang3",
    "Lang4",
    "Lang5",
    "Lang6",
    "Lang7",
    "Lang8",
    "Lang9",
    "Alt Erase",
    "SysReq",
    "Cancel",
    "Clear",
    "Prior",
    "Return2",
    "Separator",
    "Out",
    "Oper",
    "ClearAgain",
    "Crsel",
    "Exsel",
    "[unset]",
    "[unset]",
    "[unset]",
    "[unset]",
    "[unset]",
    "[unset]",
    "[unset]",
    "[unset]",
    "[unset]",
    "[unset]",
    "[unset]",
    "Keypad 00",
    "Keypad 000",
    "ThousSeparat",
    "DecSeparat",
    "CurrencyUnit",
    "CurrencySubUnit",
    "Keypad (",
    "Keypad )",
    "Keypad {",
    "Keypad }",
    "Keypad Tab",
    "Keypad Backspace",
    "Keypad A",
    "Keypad B",
    "Keypad C",
    "Keypad D",
    "Keypad E",
    "Keypad F",
    "Keypad XOR",
    "Keypad Power",
    "Keypad %",
    "Keypad <",
    "Keypad >",
    "Keypad &",
    "Keypad &&",
    "Keypad |",
    "Keypad ||",
    "Keypad :",
    "Keypad #",
    "Keypad Space",
    "Keypad @",
    "Keypad !",
    "Keypad MemStr",
    "Keypad MemRec",
    "Keypad MemClr",
    "Keypad Mem+",
    "Keypad Mem-",
    "Keypad Mem*",
    "Keypad Mem/",
    "Keypad +/-",
    "Keypad Clear",
    "Keypad ClearEntry",
    "Keypad Binary",
    "Keypad Octal",
    "Keypad Dec",
    "Keypad HexaDec",
    "[unset]",
    "[unset]",
    "LCtrl",
    "LShift",
    "LAlt",
    "Win/Cmd",
    "RCtrl",
    "RShift",
    "RAlt",
    "RWin/Cmd",
};

namespace gui {

static void check_for_sdlrange(GuiState &gui, HostState &host) {
    if (host.cfg.keyboard_button_select	< 0 || host.cfg.keyboard_button_select > 231) {
        host.cfg.keyboard_button_select = 0; 
	}
    if (host.cfg.keyboard_button_start < 0 || host.cfg.keyboard_button_start > 231) {
        host.cfg.keyboard_button_start = 0;
    }
    if (host.cfg.keyboard_button_up < 0 || host.cfg.keyboard_button_up > 231) {
        host.cfg.keyboard_button_up = 0;
    }
    if (host.cfg.keyboard_button_right < 0 || host.cfg.keyboard_button_right > 231) {
        host.cfg.keyboard_button_right = 0;
    }
    if (host.cfg.keyboard_button_down < 0 || host.cfg.keyboard_button_down > 231) {
        host.cfg.keyboard_button_down = 0;
    }
    if (host.cfg.keyboard_button_left < 0 || host.cfg.keyboard_button_left > 231) {
        host.cfg.keyboard_button_left = 0;
    }
    if (host.cfg.keyboard_button_l1 < 0 || host.cfg.keyboard_button_l1 > 231) {
        host.cfg.keyboard_button_l1 = 0;
    }
    if (host.cfg.keyboard_button_r1 < 0 || host.cfg.keyboard_button_r1 > 231) {
        host.cfg.keyboard_button_r1 = 0;
    }
    if (host.cfg.keyboard_button_l2 < 0 || host.cfg.keyboard_button_l2 > 231) {
        host.cfg.keyboard_button_l2 = 0;
    }
    if (host.cfg.keyboard_button_r2 < 0 || host.cfg.keyboard_button_r2 > 231) {
        host.cfg.keyboard_button_r2 = 0;
    }
    if (host.cfg.keyboard_button_l3 < 0 || host.cfg.keyboard_button_l3 > 231) {
        host.cfg.keyboard_button_l3 = 0;
    }
    if (host.cfg.keyboard_button_r3 < 0 || host.cfg.keyboard_button_r3 > 231) {
        host.cfg.keyboard_button_r3 = 0;
    }
    if (host.cfg.keyboard_button_triangle < 0 || host.cfg.keyboard_button_triangle > 231) {
        host.cfg.keyboard_button_triangle = 0;
    }
    if (host.cfg.keyboard_button_circle < 0 || host.cfg.keyboard_button_circle > 231) {
        host.cfg.keyboard_button_circle = 0;
    }
    if (host.cfg.keyboard_button_cross < 0 || host.cfg.keyboard_button_cross > 231) {
        host.cfg.keyboard_button_cross = 0;
    }
    if (host.cfg.keyboard_button_square < 0 || host.cfg.keyboard_button_square > 231) {
        host.cfg.keyboard_button_square = 0;
    }
    if (host.cfg.keyboard_leftstick_left < 0 || host.cfg.keyboard_leftstick_left > 231) {
        host.cfg.keyboard_leftstick_left = 0;
    }
    if (host.cfg.keyboard_leftstick_right < 0 || host.cfg.keyboard_leftstick_right > 231) {
        host.cfg.keyboard_leftstick_right = 0;
    }
    if (host.cfg.keyboard_leftstick_up < 0 || host.cfg.keyboard_leftstick_up > 231) {
        host.cfg.keyboard_leftstick_up = 0;
    }
    if (host.cfg.keyboard_leftstick_down < 0 || host.cfg.keyboard_leftstick_down > 231) {
        host.cfg.keyboard_leftstick_down = 0;
    }
    if (host.cfg.keyboard_rightstick_left < 0 || host.cfg.keyboard_rightstick_left > 231) {
        host.cfg.keyboard_rightstick_left = 0;
    }
    if (host.cfg.keyboard_rightstick_right < 0 || host.cfg.keyboard_rightstick_right > 231) {
        host.cfg.keyboard_rightstick_right = 0;
    }
    if (host.cfg.keyboard_rightstick_up < 0 || host.cfg.keyboard_rightstick_up > 231) {
        host.cfg.keyboard_rightstick_up = 0;
    }
    if (host.cfg.keyboard_rightstick_down < 0 || host.cfg.keyboard_rightstick_down > 231) {
        host.cfg.keyboard_rightstick_down = 0;
    }
    if (host.cfg.keyboard_button_psbutton < 0 || host.cfg.keyboard_button_psbutton > 231) {
        host.cfg.keyboard_button_psbutton = 0;
    }
}

static void remapper_button(GuiState &gui, HostState &host, int *button) {
    if (ImGui::Button(SDL_key_to_string[*button])) {
        gui.oldCapturedKey = *button;
        gui.isCapturingKeys = true;
        while (gui.isCapturingKeys == true) {
            handle_events(host, gui);
            *button = gui.capturedKey;
            check_for_sdlrange(gui, host);
        }
		config::serialize_config(host.cfg, host.cfg.config_path);
    }
}

void draw_controls_dialog(GuiState &gui, HostState &host) {
    check_for_sdlrange(gui, host);
    float width = ImGui::GetWindowWidth() / 1.35f;
    float height = ImGui::GetWindowHeight() / 1.25f;
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowPosCenter();
    ImGui::Begin("Controls", &gui.help_menu.controls_dialog);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s    %-16s", "Button", "Mapped button");
    ImGui::Text("%-16s", "Left stick up");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_leftstick_up);
    ImGui::Text("%-16s", "Left stick down");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_leftstick_down);
    ImGui::Text("%-16s", "Left stick right");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_leftstick_right);
    ImGui::Text("%-16s", "Left stick left");
	ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_leftstick_left);
    ImGui::Text("%-16s", "Right stick up");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_rightstick_up);
    ImGui::Text("%-16s", "Right stick down");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_rightstick_down);
    ImGui::Text("%-16s", "Right stick right");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_rightstick_right);
    ImGui::Text("%-16s", "Right stick left");
    ImGui::SameLine(); 
    remapper_button(gui, host, &host.cfg.keyboard_rightstick_left);
    ImGui::Text("%-16s", "D-pad up");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_up);
    ImGui::Text("%-16s", "D-pad down");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_down);
    ImGui::Text("%-16s", "D-pad right");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_right);
    ImGui::Text("%-16s", "D-pad left");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_left);
    if (host.cfg.pstv_mode) {
        ImGui::Text("%-16s", "L1 button");
        ImGui::SameLine();
        remapper_button(gui, host, &host.cfg.keyboard_button_l1);
        ImGui::Text("%-16s", "R1 button");
        ImGui::SameLine();
        remapper_button(gui, host, &host.cfg.keyboard_button_r1);
        ImGui::Text("%-16s", "L2 button");
        ImGui::SameLine();
        remapper_button(gui, host, &host.cfg.keyboard_button_l2);
        ImGui::Text("%-16s", "R2 button");
        ImGui::SameLine();
        remapper_button(gui, host, &host.cfg.keyboard_button_r2);
        ImGui::Text("%-16s", "L3 button");
        ImGui::SameLine();
        remapper_button(gui, host, &host.cfg.keyboard_button_l3);
        ImGui::Text("%-16s", "R3 button");
        ImGui::SameLine();
        remapper_button(gui, host, &host.cfg.keyboard_button_r3);
    } else {
        ImGui::Text("%-16s", "L button");
        ImGui::SameLine();
        remapper_button(gui, host, &host.cfg.keyboard_button_l1);
        ImGui::Text("%-16s", "R button");
        ImGui::SameLine();
        remapper_button(gui, host, &host.cfg.keyboard_button_r1);
    }
    ImGui::Text("%-16s", "Square button");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_square);
    ImGui::Text("%-16s", "Cross button");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_cross);
    ImGui::Text("%-16s", "Circle button");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_circle);
    ImGui::Text("%-16s", "Triangle button");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_triangle);
    ImGui::Text("%-16s", "Start button");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_start);
    ImGui::Text("%-16s", "Select button");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_select);
    ImGui::Text("%-16s", "PS button");
    ImGui::SameLine();
    remapper_button(gui, host, &host.cfg.keyboard_button_psbutton);

    //ImGui::Separator();
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%-16s", "GUI");
    ImGui::Text("%-16s    %-16s", "Toggle Touch", "T");
    ImGui::Text("%-16s    %-16s", "Toggle GUI visibility", "G");

    ImGui::End();
}

} // namespace gui
