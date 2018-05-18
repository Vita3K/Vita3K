// Fragment shader.
#version 410

uniform vec4 uTintColor;
uniform   sampler2D tex;

in vec2 vTexcoord;

out vec4 fragColor;

void main() {
    fragColor = texture(tex, vTexcoord) * uTintColor;
}
