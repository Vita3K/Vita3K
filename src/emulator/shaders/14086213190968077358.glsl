// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/modulate_rgba_alpha_fog_f.cg
#version 120

uniform sampler2D tex;
varying vec4 vColor;
varying vec2 vTexcoord;
varying float vFog;

void main()
{
    vec4 c = texture2D(tex, vTexcoord) * vColor;
    c.rgb = c.rgb * vFog;
    if (c.a > 0.666) gl_FragColor = c;
    else discard;
}