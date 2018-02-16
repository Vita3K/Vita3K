// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/texture_v.cg
#version 120

uniform mat4 wvp;
attribute vec3 aPosition;
attribute vec2 aTexcoord;

varying vec2 vTexcoord;

void main()
{
    gl_Position = wvp * vec4(aPosition, 1);
    vTexcoord = aTexcoord;
}
