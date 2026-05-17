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
// Copyright RPCS3
// SPDX-License-Identifier: GPL-2.0
// Code heavily referenced/taken from https://github.com/RPCS3/rpcs3/tree/master/rpcs3/Emu/RSX/Overlays

#pragma once

#include <overlay/overlay.h>

#include <array>
#include <functional>

namespace overlay {

// Aggregated button state array. Index with static_cast<size_t>(pad_button).
using button_states = std::array<bool, static_cast<size_t>(pad_button::pad_button_max_enum)>;

// Callback that polls all connected input sources (gamepads + keyboard)
// and returns the merged button state mapped to pad_button values.
using poll_input_callback_t = std::function<button_states()>;

// Touch/mouse state for overlay hit-testing.
struct overlay_touch_state {
    float x = 0.f; // virtual viewport X (0..960)
    float y = 0.f; // virtual viewport Y (0..544)
    bool pressed = false;
};

using poll_touch_callback_t = std::function<overlay_touch_state()>;
using set_intercept_callback_t = std::function<void(bool intercepted)>;

class overlay_input_handler {
public:
    overlay_input_handler(poll_input_callback_t poll_fn, set_intercept_callback_t intercept_fn,
        poll_touch_callback_t touch_fn = nullptr);

    // Returns current button states from all input sources.
    button_states poll();

    // Returns current touch/mouse state.
    overlay_touch_state poll_touch();

    // Enable / disable game input interception.
    void set_intercepted(bool intercepted);

    std::function<bool()> is_paused;
    std::function<bool()> is_abort_requested;

private:
    poll_input_callback_t m_poll_fn;
    set_intercept_callback_t m_intercept_fn;
    poll_touch_callback_t m_touch_fn;
};

} // namespace overlay
