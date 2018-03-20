// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/texture_tint_f.cg
#version 120

uniform sampler2D tex;
uniform vec4 uTintColor;
varying vec2 vTexcoord;

void main()
{
    gl_FragColor = texture2D(tex, vTexcoord) * uTintColor;
}
