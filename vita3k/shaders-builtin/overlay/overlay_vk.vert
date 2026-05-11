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
// Shader code heavily referenced from https://github.com/RPCS3/rpcs3/tree/master/rpcs3/Emu/RSX/Overlays

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 in_pos; // xy = position, zw = texcoord

layout(location = 0) out vec2 tc0;
layout(location = 1) out vec4 color;
layout(location = 2) out vec4 clip_rect;

layout(push_constant) uniform PushConstants {
    vec4 ui_scale;
    vec4 albedo;
    vec4 viewport;
    vec4 clip_bounds;
    uint vertex_config;
    uint fragment_config;
    float timestamp;
    float blur_intensity;
    vec4 sdf_params;
    vec4 sdf_origin;
    vec4 sdf_border_color;
} pc;

vec2 snap_to_grid(vec2 normalized) {
    return floor(normalized * pc.viewport.xy + vec2(0.5)) / pc.viewport.xy;
}

vec4 clip_to_ndc(vec4 coord, bool flip_vertically) {
    vec4 ret = (coord * pc.ui_scale.zwzw) / pc.ui_scale.xyxy;
    // Vulkan: Y is already top-down, flip if flip_vertically is set
    if (flip_vertically) ret.yw = 1.0 - ret.yw;
    return ret;
}

vec4 ndc_to_window(vec4 coord) {
    return coord * pc.viewport.xyxy + pc.viewport.zwzw;
}

vec4 make_aabb(vec4 coords) {
    vec4 result = coords;
    if (coords.x > coords.z) {
        result.xz = coords.zx;
    }
    if (coords.y > coords.w) {
        result.yw = coords.wy;
    }
    return result;
}

void main() {
    bool no_vertex_snap = (pc.vertex_config & 1u) != 0u;
    bool flip_vertically = (pc.vertex_config & 2u) != 0u;

    tc0.xy = in_pos.zw;
    color = pc.albedo;
    clip_rect = make_aabb(ndc_to_window(clip_to_ndc(pc.clip_bounds, flip_vertically)));

    vec4 pos = vec4(clip_to_ndc(in_pos, flip_vertically).xy, 0.5, 1.0);

    if (!no_vertex_snap) {
        pos.xy = snap_to_grid(pos.xy);
    }

    gl_Position = (pos + pos) - 1.0;
}
