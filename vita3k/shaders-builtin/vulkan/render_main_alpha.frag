// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout(location = 0) in vec2 uv_frag;
layout(binding = 0) uniform sampler2D fb;
layout(location = 0) out vec4 color_frag;

void main() {
    color_frag = texture(fb, uv_frag);
}
