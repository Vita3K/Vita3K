// Vertex shader.
#version 120

attribute vec4 aPosition;
uniform vec4 color;
varying vec4 vColor;

void main() 
{
    gl_Position = aPosition;
    vColor = color;
}
