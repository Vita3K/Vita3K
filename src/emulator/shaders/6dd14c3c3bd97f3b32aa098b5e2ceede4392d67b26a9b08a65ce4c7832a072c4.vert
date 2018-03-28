// disable_color_buffer_v
#version 120

attribute vec3 position;
uniform mat4 u_mvp_matrix;

void main() {
    gl_Position = u_mvp_matrix * vec4(position, 1);
}
