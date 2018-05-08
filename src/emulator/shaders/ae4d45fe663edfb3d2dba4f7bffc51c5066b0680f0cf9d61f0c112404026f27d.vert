// Vertex shader.
#version 120

attribute vec4 input__in_Position;
attribute vec2 input__in_TextureCoord;

varying vec2 vTexcoord;
uniform mat4 _gm_Matrices[5];

void main() {
    vec4 _object_space_pos = vec4(input__in_Position.x, input__in_Position.y, input__in_Position.z, 1.0);
    gl_Position = _gm_Matrices[4] * _object_space_pos;
    vTexcoord = input__in_TextureCoord;
}