// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#pragma once

#include <app/session_controller.h>
#include <dialog/state.h>
#include <overlay/controls.h>
#include <overlay/overlay.h>
#include <overlay/user_interface.h>

#include <vector>

namespace overlay {

enum pause_overlay_button_value : int {
    PAUSE_BUTTON_RESUME,
    PAUSE_BUTTON_EXIT,
};

struct pause_overlay_button {
    rounded_rect bg;
    label text;
    pause_overlay_button_value value;
};

struct pause_overlay : public user_interface {
    explicit pause_overlay(app::AppSessionController &session_controller);

    compiled_resource get_compiled() override;
    bool poll_dialog(DialogState &dialog, int date_format, int sys_button);
    void on_button_pressed(pad_button button_press, bool is_auto_repeat) override;
    void on_touch_pressed(float vx, float vy) override;

private:
    app::AppSessionController &m_session_controller;
    rounded_rect m_bg_dimmer;
    rounded_rect m_card;
    label m_title;
    std::vector<pause_overlay_button> m_buttons;
    pause_overlay_button_value m_selected_button = PAUSE_BUTTON_RESUME;
    int m_sys_button = 0;
    float m_focus_pulse_time = 0.f;

    void layout();
    bool is_confirm_button(pad_button button) const;
    bool is_cancel_button(pad_button button) const;
    void draw_focus_ring(compiled_resource &res) const;

    static constexpr uint16_t k_card_w = 360;
    static constexpr uint16_t k_card_h = 182;
    static constexpr uint16_t k_card_radius = 8;
    static constexpr uint16_t k_title_font_size = 20;
    static constexpr uint16_t k_button_w = 280;
    static constexpr uint16_t k_button_h = 46;
    static constexpr uint16_t k_button_gap = 12;
    static constexpr uint16_t k_button_radius = 10;
    static constexpr uint16_t k_font_size = 16;
    static constexpr int16_t k_text_voffset = -3;

    static constexpr color4f k_color_bg_dim{ 0.f, 0.f, 0.f, 0.47f };
    static constexpr color4f k_color_card_bg{ 0.145f, 0.157f, 0.176f, 0.95f };
    static constexpr color4f k_color_btn_normal{ 1.f, 1.f, 1.f, 0.078f };
    static constexpr color4f k_color_btn_selected{ 1.f, 1.f, 1.f, 0.14f };
    static constexpr color4f k_color_text{ 1.f, 1.f, 1.f, 1.f };
    static constexpr color4f k_color_focus_ring{ 0.180f, 0.765f, 0.878f, 1.f };
    static constexpr uint8_t k_focus_ring_thickness = 2;
};

} // namespace overlay
