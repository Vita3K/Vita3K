// Fragment shader.
#version 410

uniform mat4 _u_colourArray;
uniform sampler2D _gm_BaseTexture;

in vec2 vTexcoord;

out vec4 fragColor;

void main() {
    vec4 c = _u_colourArray * texture(_gm_BaseTexture, vTexcoord);
    fragColor = c;
}
