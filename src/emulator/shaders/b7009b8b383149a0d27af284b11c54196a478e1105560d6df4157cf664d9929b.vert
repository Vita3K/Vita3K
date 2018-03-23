// texture2d_fog_v?
// texture2d_v?
#version 120

attribute vec3 position;
attribute vec2 texcoord;
uniform mat4 wvp;

varying vec2 vTexcoord;

void main() {
    gl_Position = vec4(position, 1) * wvp;
    vTexcoord = texcoord;
}
