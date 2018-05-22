// Vertex shader.
#version 410

in vec3 aPosition;
in vec3 aColor;
uniform mat4 wvp;

out vec4 vColor;

void main() {
	vColor = vec4(aColor, 1);
    gl_Position = vec4(aPosition, 1) * wvp;
}
