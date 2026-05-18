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

#include <overlay/animation.h>
#include <overlay/compiled_resource.h>

#include <cmath>

namespace overlay {

uint64_t animation_base::get_timestamp_us() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void animation_base::begin_animation(uint64_t timestamp_us) {
    timestamp_start_us = timestamp_us;
    timestamp_end_us = timestamp_us + get_total_duration_us();
}

uint64_t animation_base::get_total_duration_us() const {
    return static_cast<uint64_t>(duration_sec * 1'000'000.f);
}

uint64_t animation_base::get_remaining_duration_us(uint64_t timestamp_us) const {
    return timestamp_us >= timestamp_end_us ? 0 : (timestamp_end_us - timestamp_us);
}

float animation_base::get_progress_ratio(uint64_t timestamp_us) const {
    if (!timestamp_start_us)
        return 0.f;

    float t = static_cast<float>(timestamp_us - timestamp_start_us) / static_cast<float>(timestamp_end_us - timestamp_start_us);

    switch (type) {
    case animation_type::linear:
        break;
    case animation_type::ease_in_quad:
        t = t * t;
        break;
    case animation_type::ease_out_quad:
        t = t * (2.0f - t);
        break;
    case animation_type::ease_in_out_cubic:
        t = t > 0.5f ? 4.0f * std::pow((t - 1.0f), 3.0f) + 1.0f : 4.0f * std::pow(t, 3.0f);
        break;
    case animation_type::ease_out_back: {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.f;
        const float tm1 = t - 1.f;
        t = 1.f + c3 * tm1 * tm1 * tm1 + c1 * tm1 * tm1;
        break;
    }
    }

    return t;
}

void animation_translate::reset(uint64_t start_timestamp_us) {
    active = false;
    current = start;
    timestamp_start_us = start_timestamp_us;

    if (timestamp_start_us > 0) {
        timestamp_end_us = timestamp_start_us + get_total_duration_us();
    }
}

void animation_translate::apply(compiled_resource &resource) {
    if (!active)
        return;

    const vertex delta = { current.x, current.y, current.z, 0.f };
    for (auto &cmd : resource.draw_commands) {
        for (auto &v : cmd.verts) {
            v += delta;
        }
    }
}

void animation_translate::update(uint64_t timestamp_us) {
    if (!active)
        return;

    if (timestamp_start_us == 0) {
        start = current;
        begin_animation(timestamp_us);
        return;
    }

    if (timestamp_us >= timestamp_end_us) {
        finish();
        return;
    }

    float t = get_progress_ratio(timestamp_us);
    current = lerp(start, end, t);
}

void animation_translate::finish() {
    active = false;
    timestamp_start_us = 0;
    timestamp_end_us = 0;
    current = end;

    if (on_finish) {
        on_finish();
    }
}

void animation_color_interpolate::reset(uint64_t start_timestamp_us) {
    active = false;
    current = start;
    timestamp_start_us = start_timestamp_us;

    if (timestamp_start_us > 0) {
        timestamp_end_us = timestamp_start_us + get_total_duration_us();
    }
}

void animation_color_interpolate::apply(compiled_resource &resource) {
    if (!active)
        return;

    for (auto &cmd : resource.draw_commands) {
        cmd.config.color *= current;
    }
}

void animation_color_interpolate::update(uint64_t timestamp_us) {
    if (!active)
        return;

    if (timestamp_start_us == 0) {
        start = current;
        begin_animation(timestamp_us);
        return;
    }

    if (timestamp_us >= timestamp_end_us) {
        finish();
        return;
    }

    float t = get_progress_ratio(timestamp_us);
    current = lerp(start, end, t);
}

void animation_color_interpolate::finish() {
    active = false;
    timestamp_start_us = 0;
    timestamp_end_us = 0;
    current = end;

    if (on_finish) {
        on_finish();
    }
}

void animation_scale::reset(uint64_t start_timestamp_us) {
    active = false;
    current = start;
    timestamp_start_us = start_timestamp_us;

    if (timestamp_start_us > 0) {
        timestamp_end_us = timestamp_start_us + get_total_duration_us();
    }
}

void animation_scale::apply(compiled_resource &resource) {
    if (!active)
        return;

    for (auto &cmd : resource.draw_commands) {
        for (auto &v : cmd.verts) {
            v.values[0] = pivot_x + (v.values[0] - pivot_x) * current;
            v.values[1] = pivot_y + (v.values[1] - pivot_y) * current;
        }

        if (cmd.config.active_effect == compiled_resource::effect_type::sdf) {
            auto &sdf = cmd.config.effect.sdf;
            if (sdf.func != sdf_function::none) {
                sdf.cx = pivot_x + (sdf.cx - pivot_x) * current;
                sdf.cy = pivot_y + (sdf.cy - pivot_y) * current;
                sdf.hx *= current;
                sdf.hy *= current;
                sdf.br *= current;
            }
        }
    }
}

void animation_scale::update(uint64_t timestamp_us) {
    if (!active)
        return;

    if (timestamp_start_us == 0) {
        start = current;
        begin_animation(timestamp_us);
        return;
    }

    if (timestamp_us >= timestamp_end_us) {
        finish();
        return;
    }

    float t = get_progress_ratio(timestamp_us);
    current = lerp(start, end, t);
}

void animation_scale::finish() {
    active = false;
    timestamp_start_us = 0;
    timestamp_end_us = 0;
    current = end;

    if (on_finish) {
        on_finish();
    }
}

} // namespace overlay
