// Vertex shader.
#version 410

in vec3 position;
uniform mat4 u_mvp_matrix;

void main() {
    gl_Position = vec4(position, 1) * u_mvp_matrix;
}
