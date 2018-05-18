// Vertex shader.

#version 410

in vec4 inputaPosition;
in vec4 inputaColour;
in vec2 inputaTexCoord;

uniform mat4 gm_Matrices[5];

out vec4 vColor;
out vec2 vTexCoord;

void main() 
{
    gl_Position = gm_Matrices[4] * inputaPosition;
    vColor = inputaColour;
    vTexCoord = inputaTexCoord;
}
