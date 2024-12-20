// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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
#include <config/state.h>
#include <dialog/state.h>
#include <emuenv/state.h>
#include <gui/functions.h>
#include <interface.h>

namespace gui {

static constexpr std::array<const char *, 256> SDL_key_to_string{ "[unset]", "[unknown]", "[unknown]", "[unknown]", "A", "B", "C", "D", "E", "F", "G",
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

#define CALC_KEYBOARD_MEMBERS(option_type, option_name, option_default, member_name) \
    +(std::string_view(#member_name).starts_with("keyboard_") ? 1 : 0) // NOLINT(bugprone-macro-parentheses)

static constexpr short total_key_entries = 0 CONFIG_INDIVIDUAL(CALC_KEYBOARD_MEMBERS);
#undef CALC_KEYBOARD_MEMBERS

template <typename T>
int int_or_zero(T value) {
    static_assert(std::is_same_v<T, int>);
    if constexpr (std::is_same_v<T, int>)
        return value;
    else
        return 0;
}
static void prepare_map_array(EmuEnvState &emuenv, std::array<int, total_key_entries> &map) {
    size_t i = 0;
#define ADD_KEYBOARD_MEMBERS(option_type, option_name, option_default, member_name) \
    if constexpr (std::string_view(#member_name).starts_with("keyboard_")) {        \
        map[i++] = int_or_zero(emuenv.cfg.member_name);                             \
    }

    CONFIG_INDIVIDUAL(ADD_KEYBOARD_MEMBERS)
#undef ADD_KEYBOARD_MEMBERS
}

static bool need_open_error_duplicate_key_popup = false;

static void remapper_button(GuiState &gui, EmuEnvState &emuenv, int *button, const char *button_name, const char *tooltip = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", button_name);
    if (tooltip)
        SetTooltipEx(tooltip);
    ImGui::TableSetColumnIndex(1);
    // the association of the key
    int key_association = *button;
    ImGui::PushID(button_name);
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
            else if (gui.is_key_capture_dropped || (!gui.is_capturing_keys && *button != key_association && vector_utils::contains(original_state, *button))) {
                // undo the changes
                *button = key_association;
                gui.is_key_capture_dropped = false;
                need_open_error_duplicate_key_popup = true;
            }
        }
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    ImGui::PopID();
}

void draw_controls_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    static const auto BUTTON_SIZE = ImVec2(120.f * emuenv.manual_dpi_scale, 0.f);

    float height = emuenv.logical_viewport_size.y / emuenv.manual_dpi_scale;
    if (ImGui::BeginMainMenuBar()) {
        height = height - ImGui::GetWindowHeight() * 2;
        ImGui::EndMainMenuBar();
    }

    auto &lang = gui.lang.controls;
    auto &common = emuenv.common_dialog.lang.common;

    ImGui::SetNextWindowSize(ImVec2(0, height));
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##controls", &gui.controls_menu.controls_dialog, ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang["title"].c_str());
    ImGui::Spacing();
    ImGui::Separator();

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
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_gui_toggle_touch, lang["toggle_touch"].c_str(), lang["toggle_touch_description"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_gui_toggle_gui, lang["toggle_gui_visibility"].c_str(), lang["toggle_gui_visibility_description"].c_str());
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", lang["miscellaneous"].c_str());
    if (ImGui::BeginTable("misc", 2)) {
        ImGui::TableSetupColumn("button");
        ImGui::TableSetupColumn("mapped_button");
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_toggle_texture_replacement, lang["toggle_texture_replacement"].c_str());
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_take_screenshot, lang["take_a_screenshot"].c_str());
        ImGui::EndTable();
    }

    if (need_open_error_duplicate_key_popup) {
        ImGui::OpenPopup(gui.lang.controls["error"].c_str());
        need_open_error_duplicate_key_popup = false;
    }
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(lang["error"].c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", lang["error_duplicate_key"].c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (BUTTON_SIZE.x / 2.f));
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (BUTTON_SIZE.x / 2.f));
    if (ImGui::Button(common["close"].c_str(), BUTTON_SIZE))
        gui.controls_menu.controls_dialog = false;

    ImGui::End();
}

} // namespace gui
