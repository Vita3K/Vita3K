// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/rgba_v.cg
#version 120

uniform mat4 wvp;
attribute vec3 aPosition;
attribute vec4 aColor;

varying vec4 vColor;

void main()
{
    gl_Position = vec4(aPosition, 1) * wvp;
    vColor = aColor;
}
