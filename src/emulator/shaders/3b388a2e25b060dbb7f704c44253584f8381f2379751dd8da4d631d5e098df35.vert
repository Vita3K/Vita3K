// Vertex shader.

#version 120

attribute vec4 input_aPosition;
attribute vec4 input_aColour;
attribute vec2 input_aTexCoord;

uniform mat4 gm_Matrices[5];

varying vec4 vColor;
varying vec2 vTexCoord;

void main() 
{
    gl_Position = gm_Matrices[4] * input_aPosition;
    vColor = input_aColour;
    vTexCoord = input_aTexCoord;
}
