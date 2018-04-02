// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/modulate_rgba_f.cg
#version 120

uniform sampler2D tex;
varying vec4 vColor;
varying vec2 vTexcoord;

void main()
{
    gl_FragColor = texture2D(tex, vTexcoord) * vColor;
}
