// Vertex shader.
#version 410

in vec3 position;
in vec2 texcoord;
uniform mat4 wvp;

out vec2 vTexcoord;

void main() {
	vTexcoord = texcoord;
    gl_Position = vec4(position, 1) * wvp;
}
