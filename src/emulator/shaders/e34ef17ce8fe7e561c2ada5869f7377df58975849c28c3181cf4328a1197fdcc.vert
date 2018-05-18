// Vertex shader.
#version 410

in vec3 aPosition;
in vec2 aTexcoord;

uniform mat4 wvp;

out vec2 vTexcoord;

void main() {
    gl_Position = wvp * vec4(aPosition, 1);
    vTexcoord = aTexcoord;
}
