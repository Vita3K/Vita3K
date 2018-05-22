// Vertex shader.
#version 410

in vec3 aPosition;
uniform mat4 wvp;

void main() {
    gl_Position = vec4(aPosition, 1) * wvp;
}
