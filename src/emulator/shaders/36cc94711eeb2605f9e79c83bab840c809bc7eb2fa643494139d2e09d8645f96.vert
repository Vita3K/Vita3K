// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/texture2d_v.cg
#version 410

uniform mat4 wvp;
in vec3 position;
in vec2 texcoord;

out vec2 vTexcoord;

void main()
{
    gl_Position = vec4(position, 1) * wvp;
    
    vTexcoord = texcoord;
}
