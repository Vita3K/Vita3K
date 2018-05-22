// Fragment shader.
#version 410

uniform vec4 vColor;
uniform sampler2D tex;

in vec2 vTexcoord;

out vec4 fragColor;

void main() {
    vec4 c = texture(tex, vTexcoord) * vColor;
    if (c.a > 0.666) fragColor = c;
    else discard;
}
