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

#include <overlay/types.h>

#include <chrono>
#include <cstdint>
#include <functional>

namespace overlay {

struct compiled_resource;

enum class animation_type {
    linear,
    ease_in_quad,
    ease_out_quad,
    ease_in_out_cubic,
    ease_out_back,
};

struct animation_base {
protected:
    uint64_t timestamp_start_us = 0;
    uint64_t timestamp_end_us = 0;

    void begin_animation(uint64_t timestamp_us);
    float get_progress_ratio(uint64_t timestamp_us) const;

    template <typename T>
    static T lerp(const T &a, const T &b, float t) {
        return (a * (1.f - t)) + (b * t);
    }

public:
    bool active = false;
    animation_type type{ animation_type::linear };
    float duration_sec = 1.f;

    std::function<void()> on_finish;

    uint64_t get_total_duration_us() const;
    uint64_t get_remaining_duration_us(uint64_t timestamp_us) const;

    virtual ~animation_base() = default;
    virtual void apply(compiled_resource &resource) = 0;
    virtual void reset(uint64_t start_timestamp_us) = 0;
    virtual void update(uint64_t timestamp_us) = 0;

    static uint64_t get_timestamp_us();
};

struct animation_translate : animation_base {
private:
    vector3f start{};

public:
    vector3f current{};
    vector3f end{};

    void apply(compiled_resource &resource) override;
    void reset(uint64_t start_timestamp_us = 0) override;
    void update(uint64_t timestamp_us) override;
    void finish();
};

struct animation_color_interpolate : animation_base {
private:
    color4f start{};

public:
    color4f current{};
    color4f end{};

    void apply(compiled_resource &resource) override;
    void reset(uint64_t start_timestamp_us = 0) override;
    void update(uint64_t timestamp_us) override;
    void finish();
};

struct animation_scale : animation_base {
private:
    float start = 0.f;

public:
    float current = 0.f;
    float end = 1.f;
    float pivot_x = 0.f;
    float pivot_y = 0.f;

    void apply(compiled_resource &resource) override;
    void reset(uint64_t start_timestamp_us = 0) override;
    void update(uint64_t timestamp_us) override;
    void finish();
};

} // namespace overlay
