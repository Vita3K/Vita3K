// texture2d_rgba_fog_v
// texture2d_rgba_v
#version 120

attribute vec3 position;
attribute vec2 texcoord;
attribute vec4 color;
uniform mat4 wvp;

varying vec2 vTexcoord;
varying vec4 vColor;

void main() {
    gl_Position = vec4(position, 1) * wvp;
    vTexcoord = texcoord;
    vColor = color;
}
