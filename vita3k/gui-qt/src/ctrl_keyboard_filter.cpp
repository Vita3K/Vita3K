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

#include <gui-qt/ctrl_keyboard_filter.h>
#include <gui-qt/physical_key_qt.h>

#include <config/state.h>
#include <ctrl/ctrl.h>
#include <ctrl/state.h>
#include <touch/functions.h>
#include <util/log.h>

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <mutex>

CtrlKeyboardFilter::CtrlKeyboardFilter(EmuEnvState &emuenv, QObject *parent)
    : QObject(parent)
    , m_emuenv(emuenv){};

bool CtrlKeyboardFilter::eventFilter(QObject *watched, QEvent *event) {
    switch (event->type()) {
    case QEvent::FocusOut: {
        std::lock_guard<std::mutex> lock(m_emuenv.ctrl.mutex);

        m_pressed_keys.clear();
        m_emuenv.ctrl.keyboard_state = {};
        pinch_modifier(m_emuenv.touch, false);
        pinch_automove(m_emuenv.touch, 0.0f);
        return false;
    }
    case QEvent::Wheel: {
        auto *we = static_cast<QWheelEvent *>(event);
        const float delta = we->angleDelta().y() / 120.0f;
        if (delta != 0.0f)
            pinch_move(m_emuenv.touch, delta);
        return false;
    }
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->isAutoRepeat())
            return false;

        const bool pressed = (event->type() == QEvent::KeyPress);
        const auto key = physical_key_from_qt_event(*ke);
        update_state(key, pressed);

        const auto &cfg = m_emuenv.cfg;
        const auto matches = [key](input::PhysicalKeyCode primary, input::PhysicalKeyCode alternate) {
            return key != input::PhysicalKeyCode::Unbound && (key == primary || key == alternate);
        };

        if (pressed && matches(cfg.keyboard_button_psbutton, cfg.keyboard_button_psbutton_alt))
            emit ps_button_pressed();

        if (pressed && matches(cfg.keyboard_gui_fullscreen, cfg.keyboard_gui_fullscreen_alt))
            emit fullscreen_toggled();

        if (pressed && matches(cfg.keyboard_gui_toggle_touch, cfg.keyboard_gui_toggle_touch_alt))
            toggle_touchscreen(m_emuenv.touch);

        if (pressed && matches(cfg.keyboard_toggle_texture_replacement, cfg.keyboard_toggle_texture_replacement_alt))
            emit texture_replacement_toggled();

        if (pressed && matches(cfg.keyboard_take_screenshot, cfg.keyboard_take_screenshot_alt))
            emit screenshot_requested();

        return false;
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            int w = 1, h = 1;
            if (auto *widget = qobject_cast<QWidget *>(watched)) {
                w = widget->width();
                h = widget->height();
            } else if (auto *window = qobject_cast<QWindow *>(watched)) {
                w = window->width();
                h = window->height();
            }
            const float vx = static_cast<float>(me->position().x()) / static_cast<float>(w) * 960.f;
            const float vy = static_cast<float>(me->position().y()) / static_cast<float>(h) * 544.f;
            auto &mouse = m_emuenv.ctrl.overlay_mouse;
            mouse.x.store(vx, std::memory_order_relaxed);
            mouse.y.store(vy, std::memory_order_relaxed);
            mouse.pressed.store(event->type() == QEvent::MouseButtonPress, std::memory_order_relaxed);
        }
        return false;
    }
    default:
        return QObject::eventFilter(watched, event);
    }
}

void CtrlKeyboardFilter::update_state(const input::PhysicalKeyCode key, const bool pressed) {
    const auto &cfg = m_emuenv.cfg;

    if (key == input::PhysicalKeyCode::Unbound)
        return;

    if (pressed)
        m_pressed_keys.insert(key);
    else
        m_pressed_keys.erase(key);

    const auto is_pressed = [this](input::PhysicalKeyCode primary, input::PhysicalKeyCode alternate) {
        const auto primary_it = primary != input::PhysicalKeyCode::Unbound ? m_pressed_keys.find(primary) : m_pressed_keys.end();
        if (primary_it != m_pressed_keys.end())
            return true;
        return alternate != input::PhysicalKeyCode::Unbound && m_pressed_keys.find(alternate) != m_pressed_keys.end();
    };

    float axes[4];
    axes[0] = static_cast<float>(
        -static_cast<int>(is_pressed(cfg.keyboard_leftstick_left, cfg.keyboard_leftstick_left_alt))
        + static_cast<int>(is_pressed(cfg.keyboard_leftstick_right, cfg.keyboard_leftstick_right_alt)));
    axes[1] = static_cast<float>(
        -static_cast<int>(is_pressed(cfg.keyboard_leftstick_up, cfg.keyboard_leftstick_up_alt))
        + static_cast<int>(is_pressed(cfg.keyboard_leftstick_down, cfg.keyboard_leftstick_down_alt)));
    axes[2] = static_cast<float>(
        -static_cast<int>(is_pressed(cfg.keyboard_rightstick_left, cfg.keyboard_rightstick_left_alt))
        + static_cast<int>(is_pressed(cfg.keyboard_rightstick_right, cfg.keyboard_rightstick_right_alt)));
    axes[3] = static_cast<float>(
        -static_cast<int>(is_pressed(cfg.keyboard_rightstick_up, cfg.keyboard_rightstick_up_alt))
        + static_cast<int>(is_pressed(cfg.keyboard_rightstick_down, cfg.keyboard_rightstick_down_alt)));

    uint32_t buttons = 0;
    uint32_t buttons_ext = 0;

    const auto set_bit_if_pressed = [&](input::PhysicalKeyCode primary, input::PhysicalKeyCode alternate, uint32_t bit, uint32_t &mask) {
        if (is_pressed(primary, alternate))
            mask |= bit;
    };

    auto set_common_buttons = [&](uint32_t &mask) {
        set_bit_if_pressed(cfg.keyboard_button_select, cfg.keyboard_button_select_alt, SCE_CTRL_SELECT, mask);
        set_bit_if_pressed(cfg.keyboard_button_start, cfg.keyboard_button_start_alt, SCE_CTRL_START, mask);
        set_bit_if_pressed(cfg.keyboard_button_up, cfg.keyboard_button_up_alt, SCE_CTRL_UP, mask);
        set_bit_if_pressed(cfg.keyboard_button_right, cfg.keyboard_button_right_alt, SCE_CTRL_RIGHT, mask);
        set_bit_if_pressed(cfg.keyboard_button_down, cfg.keyboard_button_down_alt, SCE_CTRL_DOWN, mask);
        set_bit_if_pressed(cfg.keyboard_button_left, cfg.keyboard_button_left_alt, SCE_CTRL_LEFT, mask);
        set_bit_if_pressed(cfg.keyboard_button_triangle, cfg.keyboard_button_triangle_alt, SCE_CTRL_TRIANGLE, mask);
        set_bit_if_pressed(cfg.keyboard_button_circle, cfg.keyboard_button_circle_alt, SCE_CTRL_CIRCLE, mask);
        set_bit_if_pressed(cfg.keyboard_button_cross, cfg.keyboard_button_cross_alt, SCE_CTRL_CROSS, mask);
        set_bit_if_pressed(cfg.keyboard_button_square, cfg.keyboard_button_square_alt, SCE_CTRL_SQUARE, mask);
        set_bit_if_pressed(cfg.keyboard_button_psbutton, cfg.keyboard_button_psbutton_alt, SCE_CTRL_PSBUTTON, mask);
    };

    set_common_buttons(buttons);
    set_common_buttons(buttons_ext);

    set_bit_if_pressed(cfg.keyboard_button_l1, cfg.keyboard_button_l1_alt, SCE_CTRL_L, buttons);
    set_bit_if_pressed(cfg.keyboard_button_r1, cfg.keyboard_button_r1_alt, SCE_CTRL_R, buttons);

    set_bit_if_pressed(cfg.keyboard_button_l1, cfg.keyboard_button_l1_alt, SCE_CTRL_L1, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r1, cfg.keyboard_button_r1_alt, SCE_CTRL_R1, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_l2, cfg.keyboard_button_l2_alt, SCE_CTRL_L2, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r2, cfg.keyboard_button_r2_alt, SCE_CTRL_R2, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_l3, cfg.keyboard_button_l3_alt, SCE_CTRL_L3, buttons_ext);
    set_bit_if_pressed(cfg.keyboard_button_r3, cfg.keyboard_button_r3_alt, SCE_CTRL_R3, buttons_ext);

    const bool pinch_modifier_active = is_pressed(cfg.keyboard_pinch_modifier, cfg.keyboard_pinch_modifier_alt)
        || is_pressed(cfg.keyboard_alternate_pinch_in, cfg.keyboard_alternate_pinch_in_alt)
        || is_pressed(cfg.keyboard_alternate_pinch_out, cfg.keyboard_alternate_pinch_out_alt);
    pinch_modifier(m_emuenv.touch, pinch_modifier_active);

    const bool pinch_in_pressed = is_pressed(cfg.keyboard_alternate_pinch_in, cfg.keyboard_alternate_pinch_in_alt);
    const bool pinch_out_pressed = is_pressed(cfg.keyboard_alternate_pinch_out, cfg.keyboard_alternate_pinch_out_alt);
    const float pinch_auto = pinch_in_pressed == pinch_out_pressed ? 0.0f : (pinch_in_pressed ? -0.5f : 0.5f);
    pinch_automove(m_emuenv.touch, pinch_auto);

    std::lock_guard<std::mutex> lock(m_emuenv.ctrl.mutex);
    auto &kb = m_emuenv.ctrl.keyboard_state;
    kb.buttons = buttons;
    kb.buttons_ext = buttons_ext;
    kb.axes[0] = axes[0];
    kb.axes[1] = axes[1];
    kb.axes[2] = axes[2];
    kb.axes[3] = axes[3];
}
