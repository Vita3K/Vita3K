// Fragment shader.
#version 120

uniform float textureEnable;
uniform sampler2D tex;

varying vec2 vTexcoord;
varying vec4 vColor;

void main() {
	if (textureEnable > 0.5f) {
		gl_FragColor = vColor * texture2D(tex, vTexcoord);
	} else {
		gl_FragColor = vColor;
	}
}
