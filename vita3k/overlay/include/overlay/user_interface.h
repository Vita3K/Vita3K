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

#include <overlay/input.h>
#include <overlay/overlay.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <set>
#include <thread>

namespace overlay {

class user_interface : public overlay {
public:
    enum selection_code {
        ok = 0,
        new_save = -1,
        canceled = -2,
        error = -3,
        interrupted = -4,
        no = -5
    };

    static constexpr uint64_t m_auto_repeat_ms_interval_default = 200;

protected:
    uint64_t m_auto_repeat_ms_interval = m_auto_repeat_ms_interval_default;
    std::set<pad_button> m_auto_repeat_buttons = {
        pad_button::dpad_up,
        pad_button::dpad_down,
        pad_button::dpad_left,
        pad_button::dpad_right,
        pad_button::ls_up,
        pad_button::ls_down,
        pad_button::ls_left,
        pad_button::ls_right,
        pad_button::rs_up,
        pad_button::rs_down,
        pad_button::rs_left,
        pad_button::rs_right
    };

    std::atomic<bool> m_stop_input_loop{ false };
    std::atomic<bool> m_interactive{ false };

    bool m_start_pad_interception = true;
    std::atomic<bool> m_stop_pad_interception{ false };
    std::atomic<bool> m_input_thread_detached{ false };

    std::thread::id m_input_loop_thread_id;
    std::atomic<bool> m_input_loop_exited{ true };

    std::function<void(int32_t status)> on_close = nullptr;

public:
    int32_t return_code = 0;

    std::function<void(uint32_t uid)> request_remove = nullptr;
    std::function<void()> release_intercept = nullptr;

    bool is_detached() const { return m_input_thread_detached.load(); }
    void detach_input() { m_input_thread_detached.store(true); }

    bool input_loop_exited() const { return m_input_loop_exited.load(); }
    void reset_input_loop() {
        m_stop_input_loop.store(false);
        m_stop_pad_interception.store(false);
    }
    void stop_input_processing(bool stop_pad_interception);

    compiled_resource get_compiled() override = 0;

    virtual void on_button_pressed(pad_button /*button_press*/, bool /*is_auto_repeat*/) {}
    virtual void on_key_pressed(uint32_t /*key_code*/, bool /*pressed*/) {}
    virtual void on_touch_pressed(float /*vx*/, float /*vy*/) {}

    virtual void close(bool use_callback, bool stop_pad_interception);

    int32_t run_input_loop(overlay_input_handler *input, std::function<bool()> check_state = nullptr);
};

} // namespace overlay
