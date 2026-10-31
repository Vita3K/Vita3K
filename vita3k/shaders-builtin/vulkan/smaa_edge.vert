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

layout(location = 0) in vec3 position_vertex;
layout(location = 1) in vec2 uv_vertex;

layout(push_constant) uniform constants {
    vec4 rt_metrics; // 1/w, 1/h, w, h
} pc;

#define SMAA_RT_METRICS pc.rt_metrics
#define SMAA_GLSL_4 1
#define SMAA_PRESET_HIGH 1
#define SMAA_INCLUDE_PS 0
#include "../SMAA.hlsl"

layout(location = 0) out vec2 uv_frag;
layout(location = 1) out vec4 offset_frag[3];

void main() {
    gl_Position = vec4(position_vertex, 1.0);
    uv_frag = uv_vertex;
    SMAAEdgeDetectionVS(uv_vertex, offset_frag);
}
