// Fragment shader.
#version 120

uniform mat4 _u_colourArray;
uniform sampler2D _gm_BaseTexture;

varying vec2 vTexcoord;

void main() {
    vec4 c = _u_colourArray * texture2D(_gm_BaseTexture, vTexcoord);
    gl_FragColor = c;
}