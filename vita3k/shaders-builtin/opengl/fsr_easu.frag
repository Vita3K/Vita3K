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

// FSR 1.0 edge adaptive upscaling pass. The FidelityFX headers and the defines
// (A_GPU, A_GLSL, FSR_EASU_F) are pasted in front of this file by ScreenRenderer.

in vec2 uv_frag;

uniform sampler2D fb;
// xy: top left of the displayed area in the source texture, zw: its size (in pixels)
uniform vec4 input_region;
uniform vec2 texture_size;
uniform vec2 output_size;

out vec4 color_frag;

// the fsr callbacks take the 4 texels textureGather would return
#ifdef FSR_EMULATE_GATHER
vec4 gather_texels(vec2 p, int channel) {
    ivec2 last = textureSize(fb, 0) - 1;
    ivec2 i0 = ivec2(floor(p * vec2(last + 1) - 0.5));
    ivec2 i1 = i0 + 1;
    i0 = clamp(i0, ivec2(0), last);
    i1 = clamp(i1, ivec2(0), last);
    return vec4(
        texelFetch(fb, ivec2(i0.x, i1.y), 0)[channel],
        texelFetch(fb, ivec2(i1.x, i1.y), 0)[channel],
        texelFetch(fb, ivec2(i1.x, i0.y), 0)[channel],
        texelFetch(fb, ivec2(i0.x, i0.y), 0)[channel]);
}
AF4 FsrEasuRF(AF2 p) { return gather_texels(p, 0); }
AF4 FsrEasuGF(AF2 p) { return gather_texels(p, 1); }
AF4 FsrEasuBF(AF2 p) { return gather_texels(p, 2); }
#else
AF4 FsrEasuRF(AF2 p) { return textureGather(fb, p, 0); }
AF4 FsrEasuGF(AF2 p) { return textureGather(fb, p, 1); }
AF4 FsrEasuBF(AF2 p) { return textureGather(fb, p, 2); }
#endif

void main() {
    AU4 con0, con1, con2, con3;
    FsrEasuConOffset(con0, con1, con2, con3, input_region.z, input_region.w, texture_size.x, texture_size.y,
        output_size.x, output_size.y, input_region.x, input_region.y);

    // gl_FragCoord counts from the bottom while the source texture starts at the top of the image
    AU2 pos = AU2(gl_FragCoord.x, output_size.y - 1.0 - floor(gl_FragCoord.y));

    AF3 c;
    FsrEasuF(c, pos, con0, con1, con2, con3);
    color_frag = vec4(c, 1.0);
}
