// https://github.com/Rinnegatamante/vitaQuake/blob/master/shaders/texture2d_rgba_fog_v.cg
#version 410

uniform mat4 wvp;
in vec3 position;
in vec2 texcoord;
in vex4 color;

out vec4 vColor;
out float vFog;
out vec2 vTexcoord;

void main()
{
    gl_Position = vec4(position, 1) * wvp;
    
    float dist = distance(gl_Position,vec4(0.0,0.0,0.0,1.0));
    vFog = ((512.0 - dist) / (512.0));
    
    vTexcoord = texcoord;
    vColor = color;
}
