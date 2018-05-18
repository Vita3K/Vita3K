// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/modulate_alpha_f.cg
#version 410

uniform sampler2D tex;
uniform vec4 vColor;
in vec2 vTexcoord;
out vec4 fragColor;

void main()
{
    vec4 c = texture(tex, vTexcoord) * vColor;
    if (c.a > 0.666) fragColor = c;
    else discard;
}
