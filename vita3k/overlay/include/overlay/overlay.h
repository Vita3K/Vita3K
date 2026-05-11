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

#include <overlay/compiled_resource.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>

namespace overlay {

// Pad button identifiers for overlay input handling.
enum class pad_button : uint32_t {
    dpad_up = 0,
    dpad_down,
    dpad_left,
    dpad_right,
    triangle,
    circle,
    square,
    cross,
    start,
    select,
    L1,
    R1,
    L2,
    R2,
    L3,
    R3,
    ls_up,
    ls_down,
    ls_left,
    ls_right,
    rs_up,
    rs_down,
    rs_left,
    rs_right,
    pad_button_max_enum
};

// Helper to generate unique type IDs for overlay subclasses
inline uint32_t next_overlay_type_id() {
    static std::atomic<uint32_t> counter{ 0 };
    return counter.fetch_add(1);
}

template <typename T>
uint32_t get_overlay_type_id() {
    static const uint32_t id = next_overlay_type_id();
    return id;
}

struct overlay {
    uint32_t uid = UINT32_MAX;
    uint32_t type_index = UINT32_MAX;

    static constexpr uint16_t virtual_width = 960;
    static constexpr uint16_t virtual_height = 544;

    // When true, coordinates are in full framebuffer space rather than virtual space. (TODO)
    bool use_window_space = false;

    std::atomic<bool> visible{ false };
    mutable std::atomic<bool> needs_redraw{ false };

    // Minimum interval between flip requests
    uint64_t min_refresh_duration_us = 16600;

    std::function<void()> flip_request;

    virtual ~overlay() = default;

    virtual void update(uint64_t /*timestamp_us*/) {}
    virtual compiled_resource get_compiled() = 0;

    // Request a redraw if the overlay is currently visible.
    void refresh();

    virtual uint16_t get_virtual_width() const { return virtual_width; }
    virtual uint16_t get_virtual_height() const { return virtual_height; }
    virtual void set_render_viewport(uint16_t /*width*/, uint16_t /*height*/) {}

private:
    std::chrono::steady_clock::time_point last_refresh_time{};
};

} // namespace overlay
