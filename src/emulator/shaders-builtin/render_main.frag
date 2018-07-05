#version 410 core

in vec2 uv_frag;

uniform sampler2D fb;

out vec3 color_frag;

void main() {
	color_frag = texture(fb, uv_frag).rgb;
}
