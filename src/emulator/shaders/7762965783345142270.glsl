// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/texture_f.cg
#version 120

uniform sampler2D tex;
varying vec2 vTexcoord;

void main()
{
    gl_FragColor = texture2D(tex, vTexcoord);
}
