// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/rgba_alpha_f.cg
#version 120

varying vec4 vColor;

void main()
{
    if (vColor.a > 0.666) gl_FragColor = vColor;
    else discard;
}