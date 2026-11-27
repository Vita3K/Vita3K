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

// SMAA pass 2: blending weight calculation.
// The SMAA.hlsl library and the rt_metrics uniform are injected as a prelude by
// ScreenRenderer::init_smaa.

in vec3 position_vertex;
in vec2 uv_vertex;

out vec2 uv_frag;
out vec2 pixcoord_frag;
out vec4 offset_frag[3];

void main() {
    gl_Position = vec4(position_vertex, 1.0);
    uv_frag = uv_vertex;
    SMAABlendingWeightCalculationVS(uv_vertex, pixcoord_frag, offset_frag);
}
