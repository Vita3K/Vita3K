// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/modulate_alpha_f.cg
#version 120

uniform sampler2D tex;
uniform vec4 vColor;
varying vec2 vTexcoord;

void main()
{
    vec4 c = texture2D(tex, vTexcoord) * vColor;
    if (c.a > 0.666) gl_FragColor = c;
    else discard;
}