// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/vertex_f.cg
#version 410

uniform vec4 color;
out vec4 fragColor;

void main()
{
	fragColor = color;
}