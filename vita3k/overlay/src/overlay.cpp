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

#include <overlay/user_interface.h>

#include <array>
#include <chrono>
#include <thread>

namespace overlay {

void overlay::refresh() {
    if (!visible.load(std::memory_order_relaxed))
        return;

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_refresh_time).count();
    if (elapsed < static_cast<int64_t>(min_refresh_duration_us))
        return;

    last_refresh_time = now;
    needs_redraw.store(true, std::memory_order_relaxed);

    if (flip_request)
        flip_request();
}

int32_t user_interface::run_input_loop(overlay_input_handler *input, std::function<bool()> check_state) {
    m_interactive.store(true);
    m_input_loop_thread_id = std::this_thread::get_id();
    m_input_loop_exited.store(false);

    constexpr uint64_t ms_threshold = 500;

    std::array<bool, static_cast<uint32_t>(pad_button::pad_button_max_enum)> last_button_state;
    last_button_state.fill(true);

    auto timestamp = std::chrono::steady_clock::now();
    auto initial_timestamp = timestamp;
    pad_button last_auto_repeat_button = pad_button::pad_button_max_enum;

    bool last_touch_pressed = false;

    const auto handle_button_press = [&](pad_button button_id, bool pressed) {
        if (button_id >= pad_button::pad_button_max_enum)
            return;

        bool &last_state = last_button_state[static_cast<uint32_t>(button_id)];

        if (pressed) {
            const bool is_auto_repeat_button = m_auto_repeat_buttons.contains(button_id);

            if (!last_state) {
                // New press — reset auto-repeat
                timestamp = std::chrono::steady_clock::now();
                initial_timestamp = timestamp;
                last_auto_repeat_button = is_auto_repeat_button ? button_id : pad_button::pad_button_max_enum;
                on_button_pressed(button_id, false);
            } else if (is_auto_repeat_button) {
                const auto now = std::chrono::steady_clock::now();
                const auto ms_since_initial = std::chrono::duration_cast<std::chrono::milliseconds>(now - initial_timestamp).count();
                const auto ms_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp).count();

                if (last_auto_repeat_button == button_id
                    && static_cast<uint64_t>(ms_since_initial) > ms_threshold
                    && static_cast<uint64_t>(ms_since_last) > m_auto_repeat_ms_interval) {
                    timestamp = now;
                    on_button_pressed(button_id, true);
                } else if (last_auto_repeat_button == pad_button::pad_button_max_enum) {
                    // An auto-repeat button was already pressed before and will now
                    // start triggering again after the next threshold.
                    last_auto_repeat_button = button_id;
                }
            }
        } else if (last_state && last_auto_repeat_button == button_id) {
            // We stopped pressing an auto-repeat button, so re-enable auto-repeat
            // for other buttons.
            last_auto_repeat_button = pad_button::pad_button_max_enum;
        }

        last_state = pressed;
    };

    // Start intercepting game input so the game receives nothing while this overlay is active.
    if (m_start_pad_interception && input) {
        input->set_intercepted(true);
    }

    int32_t result = selection_code::ok;

    while (!m_stop_input_loop) {
        if (input && input->is_abort_requested && input->is_abort_requested()) {
            result = selection_code::canceled;
            break;
        }

        if (check_state && !check_state()) {
            result = selection_code::interrupted;
            break;
        }

        // Poll actual input when an input handler is available.
        if (input) {
            const auto current = input->poll();
            for (uint32_t i = 0; i < static_cast<uint32_t>(pad_button::pad_button_max_enum); i++) {
                handle_button_press(static_cast<pad_button>(i), current[i]);
            }

            auto touch = input->poll_touch();
            if (touch.pressed && !last_touch_pressed) {
                on_touch_pressed(touch.x, touch.y);
            }
            last_touch_pressed = touch.pressed;
        }

        refresh();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Disable pad interception since this user interface has to be interactive.
    // Non-interactive user interfaces handle this in close in order to prevent
    // potential mutex issues.
    if (m_stop_pad_interception.load() && input) {
        input->set_intercepted(false);
    }

    m_interactive.store(false);
    m_input_loop_exited.store(true);
    m_input_loop_exited.notify_all();

    return result;
}

void user_interface::stop_input_processing(bool stop_pad_interception) {
    m_stop_pad_interception.store(stop_pad_interception);
    m_stop_input_loop.store(true);

    const bool interactive = m_interactive.load();
    const bool same_thread = interactive && std::this_thread::get_id() == m_input_loop_thread_id;
    if (interactive && !same_thread) {
        m_input_loop_exited.wait(false);
    }

    if (stop_pad_interception && release_intercept && (!same_thread) && !m_interactive.load()) {
        release_intercept();
    }
}

void user_interface::close(bool use_callback, bool stop_pad_interception) {
    stop_input_processing(stop_pad_interception);

    // If the input loop is running on a different thread, wait for it to fully
    // exit before firing callbacks.  When close() is called from within
    // on_button_pressed (same thread as run_input_loop), the loop will exit
    // naturally after the callback returns — no wait needed.
    if (on_close && use_callback) {
        on_close(return_code);
    }

    if (request_remove) {
        request_remove(uid);
    }
}

} // namespace overlay
