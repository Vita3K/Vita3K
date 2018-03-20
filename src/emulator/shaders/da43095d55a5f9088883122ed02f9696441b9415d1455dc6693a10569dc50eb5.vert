// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/vertex_v.cg
#version 120

uniform mat4 wvp;
attribute vec3 aPosition;

void main()
{
    gl_Position = vec4(aPosition, 1) * wvp;
}
