// vertex_v?
#version 120

attribute vec3 aPosition;
uniform mat4 wvp;

void main() {
    gl_Position = vec4(aPosition, 1) * wvp;
}
