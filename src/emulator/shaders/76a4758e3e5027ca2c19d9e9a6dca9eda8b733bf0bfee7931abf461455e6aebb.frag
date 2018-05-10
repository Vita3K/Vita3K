// Fragment shader.
#version 410

uniform float textureEnable;
uniform sampler2D tex;

in vec2 vTexcoord;
in vec4 vColor;
out vec4 fragColor;

void main() {
	if (textureEnable > 0.5f) {
		fragColor = vColor * texture(tex, vTexcoord);
	} else {
		fragColor = vColor;
	}
}
