// Fragment shader.
#version 410

uniform sampler2D gm_BaseTexture;

in vec4 vColor;
in vec2 vTexCoord;

out vec4 fragColor;

void main() 
{
    vec4 c = vColor * texture(gm_BaseTexture, vTexCoord);
    fragColor = c;
}
