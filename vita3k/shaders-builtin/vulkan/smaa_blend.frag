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

#version 450
#extension GL_GOOGLE_include_directive : require

layout(push_constant) uniform constants {
    vec4 rt_metrics; // 1/w, 1/h, w, h
} pc;

#define SMAA_RT_METRICS pc.rt_metrics
#define SMAA_GLSL_4 1
#define SMAA_PRESET_HIGH 1
#define SMAA_INCLUDE_VS 0
#include "../SMAA.hlsl"

layout(location = 0) in vec2 uv_frag;
layout(location = 1) in vec2 pixcoord_frag;
layout(location = 2) in vec4 offset_frag[3];

layout(binding = 0) uniform sampler2D edges_tex;
layout(binding = 1) uniform sampler2D area_tex;
layout(binding = 2) uniform sampler2D search_tex;

layout(location = 0) out vec4 color_out;

void main() {
    // subsampleIndices is always zero for SMAA 1x
    color_out = SMAABlendingWeightCalculationPS(uv_frag, pixcoord_frag, offset_frag,
        edges_tex, area_tex, search_tex, vec4(0.0));
}
