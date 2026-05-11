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

layout(location = 0) in vec4 in_pos; // xy = position, zw = texcoord

out vec2 tc0;
out vec4 color;
out vec4 clip_rect;

uniform vec4 ui_scale;
uniform vec4 albedo;
uniform vec4 viewport;
uniform vec4 clip_bounds;
uniform uint vertex_config;

vec2 snap_to_grid(vec2 normalized) {
    return floor(normalized * viewport.xy + vec2(0.5)) / viewport.xy;
}

vec4 clip_to_ndc(vec4 coord, bool flip_vertically) {
    vec4 ret = (coord * ui_scale.zwzw) / ui_scale.xyxy;
    // OpenGL: flip Y unless flip_vertically is set
    if (!flip_vertically) ret.yw = 1.0 - ret.yw;
    return ret;
}

vec4 ndc_to_window(vec4 coord) {
    return coord * viewport.xyxy + viewport.zwzw;
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
    bool no_vertex_snap = (vertex_config & 1u) != 0u;
    bool flip_vertically = (vertex_config & 2u) != 0u;

    tc0.xy = in_pos.zw;
    color = albedo;
    clip_rect = make_aabb(ndc_to_window(clip_to_ndc(clip_bounds, flip_vertically)));

    vec4 pos = vec4(clip_to_ndc(in_pos, flip_vertically).xy, 0.5, 1.0);

    if (!no_vertex_snap) {
        pos.xy = snap_to_grid(pos.xy);
    }

    gl_Position = (pos + pos) - 1.0;
}
