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

// FSR 1.0 contrast adaptive sharpening pass. The FidelityFX headers and the defines
// (A_GPU, A_GLSL, FSR_RCAS_F) are pasted in front of this file by ScreenRenderer.

in vec2 uv_frag;

uniform sampler2D fb;
// where the upscaled image starts in the framebuffer
uniform vec2 output_origin;
uniform float sharpness;

out vec4 color_frag;

AF4 FsrRcasLoadF(ASU2 p) { return texelFetch(fb, clamp(p, ivec2(0), textureSize(fb, 0) - 1), 0); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}

void main() {
    AU4 con;
    FsrRcasCon(con, sharpness);

    AF3 c;
    FsrRcasF(c.r, c.g, c.b, AU2(gl_FragCoord.xy - output_origin), con);
    color_frag = vec4(c, 1.0);
}
