// Vertex shader.

#version 410

attribute vec4 inputaPosition;
attribute vec4 inputaColour;
attribute vec2 inputaTexCoord;

uniform mat4 gm_Matrices[5];

out vec4 vColor;
out vec2 vTexCoord;

void main() 
{
    gl_Position = gm_Matrices[4] * inputaPosition;
    vColor = inputaColour;
    vTexCoord = inputaTexCoord;
}
