// Fragment shader.
#version 120

uniform sampler2D gm_BaseTexture;

varying vec4 vColor;
varying vec2 vTexCoord;

void main() {
    vec4 c = vColor * texture2D(gm_BaseTexture, vTexCoord);
    gl_FragColor = c;
}