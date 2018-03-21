// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/rgba_f.cg
#version 120

varying vec4 vColor;

void main()
{
    gl_FragColor = vColor;
}
