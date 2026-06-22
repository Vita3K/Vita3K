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

#define SAMPLER_MODE_NONE      0u
#define SAMPLER_MODE_FONT2D    1u
#define SAMPLER_MODE_FONT3D    2u
#define SAMPLER_MODE_TEXTURE2D 3u

#define SDF_DISABLED  0u
#define SDF_ELLIPSE   1u
#define SDF_BOX       2u
#define SDF_ROUND_BOX 3u

layout(set = 0, binding = 0) uniform sampler2D fs0;
layout(set = 0, binding = 1) uniform sampler2DArray fs1;

layout(location = 0) in vec2 tc0;
layout(location = 1) in vec4 color;
layout(location = 2) in vec4 clip_rect;

layout(location = 0) out vec4 ocol;

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

vec4 SDF_blend(float sd, float border_width, vec4 inner_color, vec4 border_color, vec4 outer_color) {
    float fw = fwidth(sd);
    float a = smoothstep(-border_width + fw, -border_width - fw, sd);
    float b = smoothstep(fw, -fw, sd);
    vec4 c = mix(outer_color, border_color, b);
    c = mix(c, inner_color, a);
    return c;
}

float SDF_fn(uint sdf) {
    vec2 p = floor(gl_FragCoord.xy) - pc.sdf_origin.xy;
    vec2 hs = pc.sdf_params.xy;
    float r = pc.sdf_params.z;
    vec2 v;

    switch (sdf) {
    case SDF_ELLIPSE:
        return (length(p / hs) - 1.0) * length(hs);
    case SDF_BOX:
        v = abs(p) - hs;
        return length(max(v, 0.0)) + min(max(v.x, v.y), 0.0);
    case SDF_ROUND_BOX:
        v = abs(p) - (hs - r);
        return length(max(v, 0.0)) + min(max(v.x, v.y), 0.0) - r;
    default:
        return -1.0;
    }
}

vec4 blur_sample(sampler2D tex, vec2 coord, vec2 tex_offset) {
    vec2 coords[9];
    coords[0] = coord - tex_offset;
    coords[1] = coord + vec2(0.0, -tex_offset.y);
    coords[2] = coord + vec2(tex_offset.x, -tex_offset.y);
    coords[3] = coord + vec2(-tex_offset.x, 0.0);
    coords[4] = coord;
    coords[5] = coord + vec2(tex_offset.x, 0.0);
    coords[6] = coord + vec2(-tex_offset.x, tex_offset.y);
    coords[7] = coord + vec2(0.0, tex_offset.y);
    coords[8] = coord + tex_offset;

    float weights[9] = float[9](
        1.0, 2.0, 1.0,
        2.0, 4.0, 2.0,
        1.0, 2.0, 1.0
    );

    vec4 blurred = vec4(0.0);
    for (int n = 0; n < 9; ++n) {
        blurred += texture(tex, coords[n]) * weights[n];
    }

    return blurred / 16.0;
}

vec4 sample_image(sampler2D tex, vec2 coord, float blur_str) {
    vec4 original = texture(tex, coord);
    if (blur_str == 0.0) return original;

    vec2 constraints = 1.0 / vec2(640.0, 360.0);
    vec2 res_offset = 1.0 / vec2(textureSize(fs0, 0));
    vec2 tex_offset = max(res_offset, constraints);

    vec4 blur0 = blur_sample(tex, coord + vec2(-res_offset.x, 0.0), tex_offset);
    vec4 blur1 = blur_sample(tex, coord + vec2(res_offset.x, 0.0), tex_offset);
    vec4 blur2 = blur_sample(tex, coord + vec2(0.0, res_offset.y), tex_offset);

    vec4 blurred = (blur0 + blur1 + blur2) / 3.0;
    return mix(original, blurred, blur_str);
}

void main() {
    bool clip_fragments = (pc.fragment_config & 1u) != 0u;
    bool use_pulse_glow = (pc.fragment_config & 2u) != 0u;
    uint sampler_mode = (pc.fragment_config >> 2u) & 3u;
    uint sdf = (pc.fragment_config >> 4u) & 3u;
    bool use_gloss = (pc.fragment_config & 64u) != 0u;
    bool use_btn_gloss = (pc.fragment_config & 128u) != 0u;

    if (clip_fragments) {
        if (gl_FragCoord.x < clip_rect.x || gl_FragCoord.x > clip_rect.z ||
            gl_FragCoord.y < clip_rect.y || gl_FragCoord.y > clip_rect.w) {
            discard;
        }
    }

    vec4 diff_color = color;
    if (use_pulse_glow) {
        diff_color.a *= (sin(pc.timestamp) + 1.0) * 0.5;
    }

    if (use_gloss) {
        float gloss_h = pc.sdf_params.x;
        float feather = pc.sdf_params.y;
        float opacity = pc.sdf_params.z;
        float u = tc0.x;
        float v = tc0.y;

        float g = v < gloss_h ? pow((gloss_h - v) / gloss_h, 1.8) : 0.0;

        float s = smoothstep(0.0, feather, u) * (1.0 - smoothstep(1.0 - feather, 1.0, u));

        diff_color = vec4(1.0, 1.0, 1.0, g * s * opacity);
    }

    if (use_btn_gloss) {
        float u = tc0.x;
        float v = tc0.y;

        float glossH, opacity, bottomOpacity, curveLift, aspect, rFrac;
        bool combined = (sdf != SDF_DISABLED);

        if (combined) {
            glossH = pc.sdf_origin.z;
            float packedVal = pc.sdf_origin.w;
            opacity = floor(packedVal) / 100.0;
            bottomOpacity = fract(packedVal);
            aspect = pc.sdf_params.x / max(pc.sdf_params.y, 0.001);
            rFrac = pc.sdf_params.z / max(pc.sdf_params.y, 0.001);
            curveLift = glossH * 0.3;
        } else {
            glossH = pc.sdf_params.x;
            curveLift = pc.sdf_params.y;
            opacity = pc.sdf_params.z;
            rFrac = pc.sdf_params.w;
            aspect = pc.sdf_origin.x;
            bottomOpacity = pc.sdf_origin.y;
        }

        float u_edge = min(u, 1.0 - u);
        float edge_frac = max(rFrac / (2.0 * max(aspect, 0.001)), 0.08);
        float edge_t = smoothstep(0.0, edge_frac, u_edge);
        float boundary = glossH - (1.0 - edge_t) * curveLift;

        float g = 0.0;
        if (v < boundary) {
            float t = v / boundary;
            g = 1.0 - 0.6 * t - 0.4 * t * t;
        }

        float bzone = 0.15;
        float bt = max(0.0, (v - (1.0 - bzone)) / bzone);
        float bottom_g = bt * bt * bottomOpacity;

        float total = max(g * opacity, bottom_g);

        if (combined) {
            diff_color = vec4(diff_color.rgb + vec3(total), diff_color.a + total);
        } else {
            vec2 p = vec2((u - 0.5) * 2.0 * aspect, (v - 0.5) * 2.0);
            vec2 hs = vec2(aspect, 1.0);
            vec2 q = abs(p) - (hs - rFrac);
            float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - rFrac;
            float mask = 1.0 - smoothstep(-fwidth(d) * 1.5, fwidth(d) * 1.5, d);
            diff_color = vec4(1.0, 1.0, 1.0, total * mask);
        }
    }

    if (sdf != SDF_DISABLED) {
        float border_w = pc.sdf_params.w;
        float d = SDF_fn(sdf);
        diff_color = SDF_blend(d, border_w, diff_color, pc.sdf_border_color, vec4(pc.sdf_border_color.rgb, 0.0));
    }

    switch (sampler_mode) {
    case SAMPLER_MODE_FONT2D:
        ocol = texture(fs0, tc0).rrrr * diff_color;
        break;
    case SAMPLER_MODE_FONT3D:
        ocol = texture(fs1, vec3(tc0.x, fract(tc0.y), trunc(tc0.y))).rrrr * diff_color;
        break;
    case SAMPLER_MODE_TEXTURE2D:
        ocol = sample_image(fs0, tc0, pc.blur_intensity).rgba * diff_color;
        break;
    default:
        ocol = diff_color;
        break;
    }
}
