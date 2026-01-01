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

void draw_overlay_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    static const auto BUTTON_SIZE = ImVec2(120.f * emuenv.manual_dpi_scale, 0.f);
    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2.f), emuenv.logical_viewport_pos.y + (display_size.y / 2.f)), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##overlay", &gui.controls_menu.overlay_dialog, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(RES_SCALE.x);

    auto &lang = gui.lang.overlay;
    auto &common = emuenv.common_dialog.lang.common;

    TextColoredCentered(GUI_COLOR_TEXT_TITLE, lang["title"].c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    static bool overlay_editing = false;

    TextColoredCentered(GUI_COLOR_TEXT_MENUBAR, lang["gamepad_overlay"].c_str());
    ImGui::Spacing();
    if (ImGui::Checkbox(lang["enable_gamepad_overlay"].c_str(), &emuenv.cfg.enable_gamepad_overlay))
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    const char *overlay_edit_text = overlay_editing ? lang["hide_gamepad_overlay"].c_str() : lang["modify_gamepad_overlay"].c_str();
    if (ImGui::Button(overlay_edit_text)) {
        overlay_editing = !overlay_editing;
        set_controller_overlay_state(overlay_editing ? get_overlay_display_mask(emuenv.cfg) : 0, overlay_editing);
    }
    ImGui::Spacing();
    if (overlay_editing && ImGui::SliderFloat(lang["overlay_scale"].c_str(), &emuenv.cfg.overlay_scale, 0.25f, 4.0f, "%.3f", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_Logarithmic)) {
        set_controller_overlay_scale(emuenv.cfg.overlay_scale);
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    ImGui::Spacing();
    if (overlay_editing && ImGui::SliderInt(lang["overlay_opacity"].c_str(), &emuenv.cfg.overlay_opacity, 0, 100, "%d%%")) {
        set_controller_overlay_opacity(emuenv.cfg.overlay_opacity);
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    if (overlay_editing && ImGui::Button(lang["reset_gamepad"].c_str())) {
        set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg), true, true);
        emuenv.cfg.overlay_scale = 1.0f;
        emuenv.cfg.overlay_opacity = 100;
        set_controller_overlay_scale(emuenv.cfg.overlay_scale);
        set_controller_overlay_opacity(emuenv.cfg.overlay_opacity);
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    ImGui::Spacing();
    ImGui::Separator();
    if (emuenv.cfg.enable_gamepad_overlay && ImGui::Checkbox(lang["overlay_show_touch_switch"].c_str(), &emuenv.cfg.overlay_show_touch_switch)) {
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg), overlay_editing);
    }
    ImGui::Text("%s", lang["l2_r2_triggers"].c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (BUTTON_SIZE.x / 2.f));
    if (ImGui::Button(common["close"].c_str(), BUTTON_SIZE)) {
        set_controller_overlay_state(0);
        overlay_editing = false;
        gui.controls_menu.overlay_dialog = false;
    }

    ImGui::End();
}
#else

void set_controller_overlay_state(int overlay_mask, bool edit, bool reset) {}
void set_controller_overlay_scale(float scale) {}
void set_controller_overlay_opacity(int opacity) {}

#endif

} // namespace gui
