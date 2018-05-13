// Fragment shader.
#version 120

uniform vec4 uTintColor;
uniform   sampler2D tex;

varying vec2 vTexcoord;

void main() {
    gl_FragColor = texture2D(tex, vTexcoord) * uTintColor;
}
