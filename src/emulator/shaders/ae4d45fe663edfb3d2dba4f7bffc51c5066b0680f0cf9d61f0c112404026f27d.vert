// Vertex shader.
#version 120

attribute vec4 input_in_Position;
attribute vec2 input_in_TextureCoord;

varying vec2 vTexcoord;
uniform mat4 _gm_Matrices[5];

void main() {
    vec4 _object_space_pos = vec4(input_in_Position.x, input_in_Position.y, input_in_Position.z, 1.0);
    gl_Position = _gm_Matrices[4] * _object_space_pos;
    vTexcoord = input_in_TextureCoord;
}