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

#include <overlay/input.h>

namespace overlay {

overlay_input_handler::overlay_input_handler(poll_input_callback_t poll_fn, set_intercept_callback_t intercept_fn,
    poll_touch_callback_t touch_fn)
    : m_poll_fn(std::move(poll_fn))
    , m_intercept_fn(std::move(intercept_fn))
    , m_touch_fn(std::move(touch_fn)) {}

button_states overlay_input_handler::poll() {
    if (m_poll_fn)
        return m_poll_fn();
    return {};
}

overlay_touch_state overlay_input_handler::poll_touch() {
    if (m_touch_fn)
        return m_touch_fn();
    return {};
}

void overlay_input_handler::set_intercepted(bool intercepted) {
    if (m_intercept_fn)
        m_intercept_fn(intercepted);
}

} // namespace overlay
