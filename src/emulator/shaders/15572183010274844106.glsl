// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/color_f.cg
#version 120

varying vec4 vColor;

void main()
{
    gl_FragColor = vColor;
}
