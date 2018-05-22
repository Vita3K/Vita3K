// Vertex shader.
#version 410

in vec2 position;

void main() {
    gl_Position = vec4(position, 1, 1);
}
