// Vertex shader.
#version 410

in vec4 aPosition;
uniform vec4 color;
out vec4 vColor;

void main() 
{
    gl_Position = aPosition;
    vColor = color;
}
