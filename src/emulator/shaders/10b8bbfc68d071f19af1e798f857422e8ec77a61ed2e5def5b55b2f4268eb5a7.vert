// Vertex shader.
#version 120

attribute vec4 inputaPosition;
attribute vec4 inputaColour;
attribute vec2 inputaTexCoord;

uniform mat4 gm_Matrices[5];

varying vec4 vColor;
varying vec2 vTexCoord;

void main() 
{
    gl_Position = gm_Matrices[4] * inputaPosition;
    vColor = inputaColour;
    vTexCoord = inputaTexCoord;
}
