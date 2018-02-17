// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/color_v.cg
#version 120

uniform mat4 wvp;
attribute vec3 aPosition;
attribute vec4 aColor;

varying vec4 vColor;

void main()
{
    gl_Position = wvp * vec4(aPosition, 1);
    vColor = aColor;
}
