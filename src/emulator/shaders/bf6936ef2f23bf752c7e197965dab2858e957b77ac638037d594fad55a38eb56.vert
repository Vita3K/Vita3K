// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/rgba_v.cg
#version 410

uniform mat4 wvp;
in vec3 aPosition;
in vec4 aColor;

out vec4 vColor;

void main()
{
    gl_Position = vec4(aPosition, 1) * wvp;
    vColor = aColor;
}
