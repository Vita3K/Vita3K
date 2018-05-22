// Fragment shader.
#version 410

uniform vec4 vColor;
uniform sampler2D tex;

in vec2 vTexcoord;

out vec4 fragColor;

void main() {
    fragColor = texture(tex, vTexcoord) * vColor;
}
