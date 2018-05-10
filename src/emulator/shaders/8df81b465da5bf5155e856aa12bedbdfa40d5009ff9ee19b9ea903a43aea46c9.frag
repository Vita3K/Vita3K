// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/modulate_rgba_f.cg
#version 410

uniform sampler2D tex;
in vec4 vColor;
in vec2 vTexcoord;
out vec4 fragColor;

void main()
{
    fragColor = texture(tex, vTexcoord) * vColor;
}
