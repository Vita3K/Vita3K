// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/texture2d_v.cg
#version 120

uniform mat4 wvp;
attribute vec3 position;
attribute vec2 texcoord;

varying vec2 vTexcoord;

void main()
{
    gl_Position = vec4(position, 1) * wvp;
    
    vTexcoord = texcoord;
}
