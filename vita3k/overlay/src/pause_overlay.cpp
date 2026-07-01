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

#include <overlay/pause_overlay.h>

#include <lang/state.h>
#include <overlay/types.h>
#include <util/log.h>
#include <util/system.h>

#include <algorithm>
#include <cmath>

namespace overlay {

namespace {
constexpr const char *k_pause_overlay_font = "ltn.pvf";
}

pause_overlay::pause_overlay(app::AppSessionController &session_controller)
    : m_session_controller(session_controller) {
    visible = true;

    m_bg_dimmer.set_pos(0, 0);
    m_bg_dimmer.set_size(virtual_width, virtual_height);
    m_bg_dimmer.back_color = k_color_bg_dim;
    m_bg_dimmer.border_radius = 0;

    m_card.back_color = k_color_card_bg;
    m_card.border_radius = k_card_radius;

    const auto &active_app_title = m_session_controller.get_active_app_title();
    m_title.set_text(active_app_title.empty() ? "" : active_app_title);
    m_title.set_font(k_pause_overlay_font, k_title_font_size, true);
    m_title.align_text(overlay_element::center);
    m_title.fore_color = k_color_text;
    m_title.back_color = { 0.f, 0.f, 0.f, 0.f };

    pause_overlay_button resume_button;
    resume_button.bg.back_color = k_color_btn_normal;
    resume_button.bg.border_radius = k_button_radius;
    resume_button.bg.border_color = { 0.f, 0.f, 0.f, 0.f };
    resume_button.text.set_text(lang::get(lang::str::resume));
    resume_button.text.set_font(k_pause_overlay_font, k_font_size, true);
    resume_button.text.align_text(overlay_element::center);
    resume_button.text.fore_color = k_color_text;
    resume_button.text.back_color = { 0.f, 0.f, 0.f, 0.f };
    resume_button.value = PAUSE_BUTTON_RESUME;
    m_buttons.push_back(std::move(resume_button));

    pause_overlay_button exit_button;
    exit_button.bg.back_color = k_color_btn_normal;
    exit_button.bg.border_radius = k_button_radius;
    exit_button.bg.border_color = { 0.f, 0.f, 0.f, 0.f };
    exit_button.text.set_text(lang::get(lang::str::exit));
    exit_button.text.set_font(k_pause_overlay_font, k_font_size, true);
    exit_button.text.align_text(overlay_element::center);
    exit_button.text.fore_color = k_color_text;
    exit_button.text.back_color = { 0.f, 0.f, 0.f, 0.f };
    exit_button.value = PAUSE_BUTTON_EXIT;
    m_buttons.push_back(std::move(exit_button));

    layout();
}

void pause_overlay::layout() {
    const int16_t card_x = static_cast<int16_t>((virtual_width - k_card_w) / 2);
    const int16_t card_y = static_cast<int16_t>((virtual_height - k_card_h) / 2);

    m_card.set_pos(card_x, card_y);
    m_card.set_size(k_card_w, k_card_h);

    // Place the title right above the card
    m_title.set_pos(static_cast<int16_t>(0),
        static_cast<int16_t>(card_y - k_title_font_size - 15));
    m_title.set_size(virtual_width, k_title_font_size + 4);

    const int16_t button_x = static_cast<int16_t>(card_x + (k_card_w - k_button_w) / 2);
    const uint16_t total_buttons_h = static_cast<uint16_t>(m_buttons.size() * k_button_h
        + (m_buttons.size() > 1 ? (m_buttons.size() - 1) * k_button_gap : 0));
    int16_t button_y = static_cast<int16_t>(card_y + (k_card_h - total_buttons_h) / 2);

    for (auto &button : m_buttons) {
        button.bg.set_pos(button_x, button_y);
        button.bg.set_size(k_button_w, k_button_h);

        button.text.set_pos(button_x,
            static_cast<int16_t>(button_y + (k_button_h - k_font_size) / 2 + k_text_voffset));
        button.text.set_size(k_button_w, k_font_size);

        button_y = static_cast<int16_t>(button_y + k_button_h + k_button_gap);
    }

    if (!m_buttons.empty()) {
        m_selected_button = (pause_overlay_button_value)std::clamp(static_cast<int>(m_selected_button), 0,
            static_cast<int>(m_buttons.size()) - 1);
    }
}

bool pause_overlay::poll_dialog(DialogState &dialog, int date_format, int sys_button) {
    static_cast<void>(dialog);
    static_cast<void>(date_format);

    m_sys_button = sys_button;
    m_focus_pulse_time += 0.032f;
    if (m_focus_pulse_time > 6.283f)
        m_focus_pulse_time -= 6.283f;

    layout();
    return true;
}

void pause_overlay::on_button_pressed(pad_button button_press, bool is_auto_repeat) {
    static_cast<void>(is_auto_repeat);

    if (m_buttons.empty())
        return;

    switch (button_press) {
    case pad_button::dpad_up:
    case pad_button::ls_up:
        m_selected_button = (pause_overlay_button_value)std::max(0, static_cast<int>(m_selected_button) - 1);
        break;
    case pad_button::dpad_down:
    case pad_button::ls_down:
        m_selected_button = (pause_overlay_button_value)std::min(static_cast<int>(m_buttons.size()) - 1, static_cast<int>(m_selected_button) + 1);
        break;
    case pad_button::circle:
    case pad_button::cross:
        [[fallthrough]]; // Its better to handle X and O below, since they can depend on settings
    default: {
        if (is_confirm_button(button_press)) {
            switch (m_selected_button) {
            case PAUSE_BUTTON_RESUME:
                m_session_controller.set_pause_reason(app::AppSessionPauseReason::User, false);
                break;
            case PAUSE_BUTTON_EXIT:
                m_session_controller.set_pause_reason(app::AppSessionPauseReason::User, false);
                m_session_controller.request_stop();
                break;
            default:
                break;
                return;
            }
        }
        if (is_cancel_button(button_press)) {
            // Handle cancel button here
        }
        break;
    }
    }
}

void pause_overlay::on_touch_pressed(float vx, float vy) {
    if (m_buttons.empty())
        return;

    const auto confirm_button = (m_sys_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE)
        ? pad_button::circle
        : pad_button::cross;

    for (size_t i = 0; i < m_buttons.size(); i++) {
        const auto &btn = m_buttons[i];
        if (vx >= btn.bg.x && vx <= btn.bg.x + btn.bg.w
            && vy >= btn.bg.y && vy <= btn.bg.y + btn.bg.h) {
            m_selected_button = btn.value;
            on_button_pressed(confirm_button, false);
            return;
        }
    }
}

compiled_resource pause_overlay::get_compiled() {
    compiled_resource result;

    if (!visible)
        return result;

    result.add(m_bg_dimmer.get_compiled());
    result.add(m_card.get_compiled());
    result.add(m_title.get_compiled());

    for (auto &button : m_buttons) {
        button.bg.back_color = (button.value == m_selected_button)
            ? k_color_btn_selected
            : k_color_btn_normal;
        result.add(button.bg.get_compiled());
        result.add(button.text.get_compiled());
    }

    draw_focus_ring(result);

    return result;
}

bool pause_overlay::is_confirm_button(pad_button button) const {
    if (m_sys_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE)
        return button == pad_button::circle;
    return button == pad_button::cross;
}

bool pause_overlay::is_cancel_button(pad_button button) const {
    if (m_sys_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE)
        return button == pad_button::cross;
    return button == pad_button::circle;
}

void pause_overlay::draw_focus_ring(compiled_resource &res) const {
    if (m_buttons.empty() || m_selected_button < 0
        || m_selected_button >= static_cast<int>(m_buttons.size())) {
        return;
    }

    const auto &selected = m_buttons[m_selected_button];
    const float pulse = 0.5f + 0.5f * std::sin(m_focus_pulse_time * 3.0f);
    const float a = 0.85f + 0.15f * pulse;

    rounded_rect ring;
    ring.set_pos(selected.bg.x, selected.bg.y);
    ring.set_size(selected.bg.w, selected.bg.h);
    ring.back_color = { k_color_focus_ring.r, k_color_focus_ring.g, k_color_focus_ring.b, 0.15f * a };
    ring.border_size = k_focus_ring_thickness;
    ring.border_color = { k_color_focus_ring.r, k_color_focus_ring.g, k_color_focus_ring.b, a };
    ring.border_radius = k_button_radius;
    res.add(ring.get_compiled());
}

} // namespace overlay
