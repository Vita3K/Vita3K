// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <util/vector_utils.h>

#ifdef __ANDROID__
#include <SDL3/SDL_system.h>
#include <jni.h>
#endif

namespace gui {

enum struct OverlayShowMask : int {
    Basic = 1, // Basic Vita Gamepad
    L2R2 = 2,
    TouchScreenSwitch = 4, // Button to switch between the front and back touchscreen
};

int get_overlay_display_mask(const Config &cfg) {
    if (!cfg.enable_gamepad_overlay)
        return 0;

    int mask = (int)OverlayShowMask::Basic;
    if (cfg.pstv_mode)
        mask |= (int)OverlayShowMask::L2R2;
    if (cfg.overlay_show_touch_switch)
        mask |= (int)OverlayShowMask::TouchScreenSwitch;

    return mask;
}

#ifdef __ANDROID__
void set_controller_overlay_state(int overlay_mask, bool edit, bool reset) {
    // retrieve the JNI environment.
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());

    // retrieve the Java instance of the SDLActivity
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz(env->GetObjectClass(activity));

    // find the identifier of the method to call
    jmethodID method_id = env->GetMethodID(clazz, "setControllerOverlayState", "(IZZ)V");

    // effectively call the Java method
    env->CallVoidMethod(activity, method_id, overlay_mask, edit, reset);

    // clean up the local references.
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);
}

void set_controller_overlay_scale(float scale) {
    // retrieve the JNI environment.
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());

    // retrieve the Java instance of the SDLActivity
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz(env->GetObjectClass(activity));

    // find the identifier of the method to call
    jmethodID method_id = env->GetMethodID(clazz, "setControllerOverlayScale", "(F)V");

    // effectively call the Java method
    env->CallVoidMethod(activity, method_id, scale);

    // clean up the local references.
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);
}

void set_controller_overlay_opacity(int opacity) {
    // retrieve the JNI environment.
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());

    // retrieve the Java instance of the SDLActivity
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz(env->GetObjectClass(activity));

    // find the identifier of the method to call
    jmethodID method_id = env->GetMethodID(clazz, "setControllerOverlayOpacity", "(I)V");

    // effectively call the Java method
    env->CallVoidMethod(activity, method_id, opacity);

    // clean up the local references.
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);
}

void draw_controls_dialog(GuiState &gui, EmuEnvState &emuenv) {
    static bool overlay_editing = false;

    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2.f), emuenv.logical_viewport_pos.y + (display_size.y / 2.f)), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("Overlay", &gui.controls_menu.controls_dialog, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(RES_SCALE.x);

    if (!gui.controls_menu.controls_dialog) {
        set_controller_overlay_state(0);
        overlay_editing = false;
    }

    ImGui::Spacing();

    const auto gmpd = ImGui::CalcTextSize("Gamepad Overlay").x;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (gmpd / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "Gamepad Overlay");
    ImGui::Spacing();
    if (ImGui::Checkbox("Show gamepad overlay ingame", &emuenv.cfg.enable_gamepad_overlay))
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    const char *overlay_edit_text = overlay_editing ? "Hide Gamepad Overlay" : "Modify Gamepad Overlay";
    if (ImGui::Button(overlay_edit_text)) {
        overlay_editing = !overlay_editing;
        set_controller_overlay_state(overlay_editing ? get_overlay_display_mask(emuenv.cfg) : 0, overlay_editing);
    }
    ImGui::Spacing();
    if (overlay_editing && ImGui::SliderFloat("Overlay scale", &emuenv.cfg.overlay_scale, 0.25f, 4.0f, "%.3f", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_Logarithmic)) {
        set_controller_overlay_scale(emuenv.cfg.overlay_scale);
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    ImGui::Spacing();
    if (overlay_editing && ImGui::SliderInt("Overlay opacity", &emuenv.cfg.overlay_opacity, 0, 100, "%d%%")) {
        set_controller_overlay_opacity(emuenv.cfg.overlay_opacity);
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    if (overlay_editing && ImGui::Button("Reset Gamepad")) {
        set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg), true, true);
        emuenv.cfg.overlay_scale = 1.0f;
        emuenv.cfg.overlay_opacity = 100;
        set_controller_overlay_scale(emuenv.cfg.overlay_scale);
        set_controller_overlay_opacity(emuenv.cfg.overlay_opacity);
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    ImGui::Spacing();
    ImGui::Separator();
    if (emuenv.cfg.enable_gamepad_overlay && ImGui::Checkbox("Show front/back touchscreen switch button.", &emuenv.cfg.overlay_show_touch_switch)) {
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg), overlay_editing);
    }
    ImGui::Text("L2/R2 triggers will be displayed only if PSTV mode is enabled.");

    ImGui::End();
}
#else

void set_controller_overlay_state(int overlay_mask, bool edit, bool reset) {}
void set_controller_overlay_scale(float scale) {}
void set_controller_overlay_opacity(int opacity) {}

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
static int int_or_zero(T value) {
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
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, gui.lang.main_menubar.controls["title"].c_str());
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
        remapper_button(gui, emuenv, &emuenv.cfg.keyboard_take_screenshot, lang["take_screenshot"].c_str());
        ImGui::EndTable();
    }

    if (need_open_error_duplicate_key_popup) {
        ImGui::OpenPopup(common["error"].c_str());
        need_open_error_duplicate_key_popup = false;
    }
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(common["error"].c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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

#endif

} // namespace gui
