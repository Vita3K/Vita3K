// Vertex shader.
#version 120

in vec2 aPosition;

void main() {
    gl_Position = vec4(aPosition, 1, 1);
}
