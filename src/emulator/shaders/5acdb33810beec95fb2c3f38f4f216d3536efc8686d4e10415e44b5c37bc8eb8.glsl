// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/texture2d_fog_v.cg
#version 120

uniform mat4 wvp;
attribute vec3 position;
attribute vec2 texcoord;

varying vec4 vColor;
varying float vFog;
varying vec2 vTexcoord;

void main()
{
    gl_Position = vec4(position, 1) * wvp;
    
    float dist = distance(gl_Position,vec4(0.0,0.0,0.0,1.0));
    vFog = ((512.0 - dist) / (512.0));
    
    vTexcoord = texcoord;
}
