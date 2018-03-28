// rgba_v?
#version 120

attribute vec3 aPosition;
attribute vec4 aColor;
uniform mat4 wvp;

varying vec4 vColor;

void main() {
    gl_Position = vec4(aPosition, 1) * wvp;
    vColor = aColor;
}
