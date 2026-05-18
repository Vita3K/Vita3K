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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>

namespace overlay {

class font;

enum image_resource_id : uint8_t {
    // NOTE: 1 - 252 are user defined
    image_resource_none = 0, // No image
    raw_image = 252, // Raw image data passed via image_info struct
    font_file = 253, // Font file
    game_icon = 254, // Use game icon
    backbuffer = 255 // Use current backbuffer contents
};

enum class primitive_type : uint8_t {
    quad_list = 0,
    triangle_strip = 1,
    line_list = 2,
    line_strip = 3,
    triangle_fan = 4
};

enum class sdf_function : uint8_t {
    none = 0,
    ellipse,
    box,
    rounded_box,
};

struct compiled_resource {
    // Effect type discriminant
    enum class effect_type : uint8_t {
        none = 0,
        sdf,
        gloss,
        btn_gloss
    };

    struct sdf_config_t {
        sdf_function func = sdf_function::none;

        float cx = 0.f; // Center x
        float cy = 0.f; // Center y
        float hx = 0.f; // Half-size in X
        float hy = 0.f; // Half-size in Y
        float br = 0.f; // Border radius
        float bw = 0.f; // Border width

        color4f border_color;

        // Optional btn_gloss params co-resident with SDF.
        float btn_gloss_height = 0.f;
        float btn_gloss_opacity = 0.f;
        float btn_gloss_bottom_opacity = 0.f;

        // Transform a SDF definition from one reference frame to another
        // Target viewport - your actual render area
        // Virtual viewport - the internal design viewport
        void transform(const areaf &target_viewport, const sizef &virtual_viewport);
    };

    struct gloss_config_t {
        float height = 1.0f;
        float feather = 0.20f;
        float opacity = 0.50f;
    };

    struct btn_gloss_config_t {
        float height = 0.54f;
        float curve_lift = 0.13f;
        float opacity = 0.87f;
        float border_radius_frac = 0.f;
        float aspect = 1.f;
        float bottom_opacity = 0.f;
    };

    struct command_config {
        primitive_type primitives = primitive_type::quad_list;

        color4f color = { 1.f, 1.f, 1.f, 1.f };
        bool pulse_glow = false;
        bool disable_vertex_snap = false;
        float pulse_sinus_offset = 0.0f;
        float pulse_speed_modifier = 0.005f;

        effect_type active_effect = effect_type::none;
        union {
            sdf_config_t sdf;
            gloss_config_t gloss;
            btn_gloss_config_t btn_gloss;
        } effect{};

        areaf clip_rect = {};
        bool clip_region = false;

        uint8_t texture_ref = image_resource_none;
        font *font_ref = nullptr;
        const void *external_data_ref = nullptr;

        uint8_t blur_strength = 0;

        command_config() = default;

        void set_image_resource(uint8_t ref);
        void set_font(font *ref);

        float get_sinus_value() const;
    };

    struct command {
        command_config config;
        std::vector<vertex> verts;
    };

    std::vector<command> draw_commands;

    void add(const compiled_resource &other);
    void add(const compiled_resource &other, float x_offset, float y_offset);
    void add(const compiled_resource &other, float x_offset, float y_offset, const areaf &clip_rect);

    void add(compiled_resource &&other);
    void add(compiled_resource &&other, float x_offset, float y_offset);
    void add(compiled_resource &&other, float x_offset, float y_offset, const areaf &clip_rect);

    void clear();

    command &append(const command &new_command);
    command &prepend(const command &new_command);
};

} // namespace overlay
