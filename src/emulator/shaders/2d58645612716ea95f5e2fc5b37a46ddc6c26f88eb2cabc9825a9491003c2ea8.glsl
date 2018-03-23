// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/replace_alpha_f.cg
#version 120

uniform sampler2D tex;
varying vec2 vTexcoord;

void main()
{
    vec4 c = texture2D(tex, vTexcoord);
    if (c.a > 0.666) gl_FragColor = c;
    else discard;
}
