// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/rgba_alpha_f.cg
#version 410

in vec4 vColor;
out vec4 fragColor;

void main()
{
    if (vColor.a > 0.666) fragColor = vColor;
    else discard;
}
