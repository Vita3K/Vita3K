// clear_v
#version 120

attribute vec2 position;

void main() {
    gl_Position = vec4(position, 1, 1);
}
