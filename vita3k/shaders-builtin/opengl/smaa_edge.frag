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

// SMAA pass 1: edge detection.
// The SMAA.hlsl library and the rt_metrics uniform are injected as a prelude by
// ScreenRenderer::init_smaa.

uniform sampler2D fb;

in vec2 uv_frag;
in vec4 offset_frag[3];

out vec4 color_frag;

void main() {
    color_frag = vec4(SMAALumaEdgeDetectionPS(uv_frag, offset_frag, fb), 0.0, 0.0);
}
