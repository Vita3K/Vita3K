// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/clear_f.cg
#version 120

uniform vec4 uClearColor;

void main()
{
    gl_FragColor = uClearColor;
}
