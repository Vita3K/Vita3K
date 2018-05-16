// Vertex shader.
#version 410

in vec3 aPosition;
in vec4 aColor;

uniform mat4 wvp;

out vec4 vColor;

void main() {
    gl_Position = wvp * vec4(aPosition, 1);
    vColor = aColor;
}
