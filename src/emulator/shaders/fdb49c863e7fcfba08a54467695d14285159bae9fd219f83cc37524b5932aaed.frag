// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/texture_f.cg
#version 410

uniform sampler2D tex;
in vec2 vTexcoord;
out vec4 fragColor;

void main()
{
    fragColor = texture(tex, vTexcoord);
}
