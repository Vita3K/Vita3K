// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/texture_v.cg
#version 410

uniform mat4 wvp;
in vec3 aPosition;
in vec2 aTexcoord;

out vec2 vTexcoord;

void main()
{
    gl_Position = wvp * vec4(aPosition, 1);
    vTexcoord = aTexcoord;
}
