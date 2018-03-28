// modulate_alpha_f?
// modulate_alpha_fog_f?
// modulate_f?
#version 120

uniform vec4 vColor;
uniform sampler2D tex;

varying vec2 vTexcoord;

void main() {
    gl_FragColor = texture2D(tex, vTexcoord) * vColor;
}
