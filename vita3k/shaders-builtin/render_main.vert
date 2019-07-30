#version 410 core

in vec3 position_vertex;
in vec2 uv_vertex;

out vec2 uv_frag;

void main() {
  gl_Position = vec4(position_vertex, 1.0);
  uv_frag = uv_vertex;
}
