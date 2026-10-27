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
#include <ctrl/functions.h>
#include <touch/functions.h>
#include <util/log.h>

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

CtrlKeyboardFilter::CtrlKeyboardFilter(EmuEnvState &emuenv, QObject *parent)
    : QObject(parent)
    , m_emuenv(emuenv) {};

bool CtrlKeyboardFilter::eventFilter(QObject *watched, QEvent *event) {
    switch (event->type()) {
    case QEvent::FocusOut: {
        m_pressed_keys.clear();
        update_keyboard_state(m_emuenv, [](input::PhysicalKeyCode) { return false; });
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

        if (pressed && matches(cfg.keyboard_gui_exit, cfg.keyboard_gui_exit_alt))
            emit exit_requested();

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
    if (key == input::PhysicalKeyCode::Unbound)
        return;

    if (pressed)
        m_pressed_keys.insert(key);
    else
        m_pressed_keys.erase(key);

    update_keyboard_state(m_emuenv, [this](input::PhysicalKeyCode held_key) {
        return m_pressed_keys.contains(held_key);
    });
}
