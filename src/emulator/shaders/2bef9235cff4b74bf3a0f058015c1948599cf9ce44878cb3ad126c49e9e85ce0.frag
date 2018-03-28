// /Users/pete/Documents/vitaQuake/shaders/modulate_alpha_f.cg ?
// /Users/pete/Documents/vitaQuake/shaders/modulate_alpha_fog_f.cg ?
// /Users/pete/Documents/vitaQuake/shaders/modulate_f.cg
#version 120

uniform vec4 vColor;
uniform sampler2D tex;

varying vec2 vTexcoord;

void main() {
    gl_FragColor = texture2D(tex, vTexcoord) * vColor;
}
