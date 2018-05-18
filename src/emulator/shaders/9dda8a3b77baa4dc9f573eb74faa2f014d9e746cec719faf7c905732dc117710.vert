// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/clear_v.cg
#version 410

in vec2 aPosition;

void main()
{
    gl_Position = vec4(aPosition, 1, 1);
}
