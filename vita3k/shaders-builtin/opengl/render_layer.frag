// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

in vec2 uv_frag;
uniform sampler2D fb;
out vec4 color_frag;

void main() {
    color_frag = texture(fb, uv_frag);
}