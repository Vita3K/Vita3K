// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/texture2d_rgba_v.cg
#version 120

uniform mat4 wvp;
attribute vec3 position;
attribute vec2 texcoord;
attribute vex4 color;

varying vec4 vColor;
varying vec2 vTexcoord;

void main()
{
    gl_Position = vec4(position, 1) * wvp;
    
    vTexcoord = texcoord;
    vColor = color;
}
