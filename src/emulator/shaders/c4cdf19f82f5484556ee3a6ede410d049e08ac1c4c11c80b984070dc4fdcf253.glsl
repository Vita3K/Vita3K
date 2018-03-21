// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/vertex_f.cg
#version 120

uniform vec4 color;

void main()
{
	gl_FragColor = color;
}