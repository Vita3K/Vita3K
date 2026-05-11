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

#include <overlay/compiled_resource.h>

namespace overlay {

void compiled_resource::sdf_config_t::transform(const areaf &target_viewport, const sizef &virtual_viewport) {
    const float scale_x = target_viewport.width() / virtual_viewport.width;
    const float scale_y = target_viewport.height() / virtual_viewport.height;

    // Ideally the average should match the x and y scaling but arithmetic drift shifts the values around a bit.
    // Also we need a way to define perfect circles when the aspect ratio is not respected.
    const float scale_av = (scale_x + scale_y) / 2;

    hx *= scale_x;
    hy *= scale_y;
    br *= scale_av;
    bw *= scale_av;

    // Border radius clamp
    br = std::min({ br, hx, hy });

    // Compute the function's origin. Account for flipped viewports as well.
    if (target_viewport.x2 < target_viewport.x1) {
        cx = target_viewport.width() - (cx * scale_x) + target_viewport.x2;
    } else {
        cx = cx * scale_x + target_viewport.x1;
    }

    if (target_viewport.y2 < target_viewport.y1) {
        cy = target_viewport.height() - (cy * scale_y) + target_viewport.y2;
    } else {
        cy = cy * scale_y + target_viewport.y1;
    }
}

void compiled_resource::command_config::set_image_resource(uint8_t ref) {
    texture_ref = ref;
    font_ref = nullptr;
}

void compiled_resource::command_config::set_font(font *ref) {
    texture_ref = font_file;
    font_ref = ref;
}

float compiled_resource::command_config::get_sinus_value() const {
    const auto now = std::chrono::steady_clock::now();
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    const float time_ms = static_cast<float>(us / 1000);
    return (time_ms * pulse_speed_modifier) - pulse_sinus_offset;
}

void compiled_resource::add(const compiled_resource &other) {
    draw_commands.insert(draw_commands.end(), other.draw_commands.begin(), other.draw_commands.end());
}

void compiled_resource::add(const compiled_resource &other, float x_offset, float y_offset) {
    auto old_size = draw_commands.size();
    draw_commands.insert(draw_commands.end(), other.draw_commands.begin(), other.draw_commands.end());

    for (size_t n = old_size; n < draw_commands.size(); ++n) {
        for (auto &v : draw_commands[n].verts) {
            v += vertex(x_offset, y_offset, 0.f, 0.f);
        }

        if (draw_commands[n].config.active_effect == effect_type::sdf
            && draw_commands[n].config.effect.sdf.func != sdf_function::none) {
            draw_commands[n].config.effect.sdf.cx += x_offset;
            draw_commands[n].config.effect.sdf.cy += y_offset;
        }
    }
}

void compiled_resource::add(const compiled_resource &other, float x_offset, float y_offset, const areaf &clip_rect) {
    auto old_size = draw_commands.size();
    draw_commands.insert(draw_commands.end(), other.draw_commands.begin(), other.draw_commands.end());

    for (size_t n = old_size; n < draw_commands.size(); ++n) {
        for (auto &v : draw_commands[n].verts) {
            v += vertex(x_offset, y_offset, 0.f, 0.f);
        }

        if (draw_commands[n].config.active_effect == effect_type::sdf
            && draw_commands[n].config.effect.sdf.func != sdf_function::none) {
            draw_commands[n].config.effect.sdf.cx += x_offset;
            draw_commands[n].config.effect.sdf.cy += y_offset;
        }

        draw_commands[n].config.clip_rect = clip_rect;
        draw_commands[n].config.clip_region = true;
    }
}

void compiled_resource::add(compiled_resource &&other) {
    draw_commands.insert(draw_commands.end(),
        std::make_move_iterator(other.draw_commands.begin()),
        std::make_move_iterator(other.draw_commands.end()));
}

void compiled_resource::add(compiled_resource &&other, float x_offset, float y_offset) {
    auto old_size = draw_commands.size();
    draw_commands.insert(draw_commands.end(),
        std::make_move_iterator(other.draw_commands.begin()),
        std::make_move_iterator(other.draw_commands.end()));

    for (size_t n = old_size; n < draw_commands.size(); ++n) {
        for (auto &v : draw_commands[n].verts) {
            v += vertex(x_offset, y_offset, 0.f, 0.f);
        }

        if (draw_commands[n].config.active_effect == effect_type::sdf
            && draw_commands[n].config.effect.sdf.func != sdf_function::none) {
            draw_commands[n].config.effect.sdf.cx += x_offset;
            draw_commands[n].config.effect.sdf.cy += y_offset;
        }
    }
}

void compiled_resource::add(compiled_resource &&other, float x_offset, float y_offset, const areaf &clip_rect) {
    auto old_size = draw_commands.size();
    draw_commands.insert(draw_commands.end(),
        std::make_move_iterator(other.draw_commands.begin()),
        std::make_move_iterator(other.draw_commands.end()));

    for (size_t n = old_size; n < draw_commands.size(); ++n) {
        for (auto &v : draw_commands[n].verts) {
            v += vertex(x_offset, y_offset, 0.f, 0.f);
        }

        if (draw_commands[n].config.active_effect == effect_type::sdf
            && draw_commands[n].config.effect.sdf.func != sdf_function::none) {
            draw_commands[n].config.effect.sdf.cx += x_offset;
            draw_commands[n].config.effect.sdf.cy += y_offset;
        }

        draw_commands[n].config.clip_rect = clip_rect;
        draw_commands[n].config.clip_region = true;
    }
}

void compiled_resource::clear() {
    draw_commands.clear();
}

compiled_resource::command &compiled_resource::append(const command &new_command) {
    draw_commands.emplace_back(new_command);
    return draw_commands.back();
}

compiled_resource::command &compiled_resource::prepend(const command &new_command) {
    draw_commands.emplace(draw_commands.begin(), new_command);
    return draw_commands.front();
}

} // namespace overlay
