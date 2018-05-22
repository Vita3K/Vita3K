// Fragment shader.
#version 410

uniform float alphaCut;
uniform int alphaOp;
uniform int texEnv;
uniform vec4 texEnvColor;
uniform sampler2D tex;

in vec2 vTexcoord;
in vec4 vColor;
out vec4 fragColor;

void main() {
	vec4 texColor = texture(tex, vTexcoord);
	
	// Texture Environment
	if (texEnv < 4){
		if (texEnv == 0){ // GL_MODULATE
			texColor = texColor * vColor;
		}else if (texEnv == 1){ // GL_DECAL
			texColor.rgb = mix(vColor.rgb, texColor.rgb, texColor.a);
			texColor.a = vColor.a;
		}else if (texEnv == 2){ // GL_BLEND
			texColor.rgb = mix(vColor.rgb, texEnvColor.rgb, texColor.rgb);
			texColor.a = texColor.a * vColor.a;
		}else{ // GL_ADD
			texColor.rgb = texColor.rgb + vColor.rgb;
			texColor.a = texColor.a * vColor.a;
		}
	}
	
	// Alpha Test
	if (alphaOp < 7){
		if (alphaOp == 0){
			if (texColor.a < alphaCut){
				discard;
			}
		}else if (alphaOp == 1){
			if (texColor.a <= alphaCut){
				discard;
			}
		}else if (alphaOp == 2){
			if (texColor.a == alphaCut){
				discard;
			}
		}else if (alphaOp == 3){
			if (texColor.a != alphaCut){
				discard;
			}
		}else if (alphaOp == 4){
			if (texColor.a > alphaCut){
				discard;
			}
		}else if (alphaOp == 5){
			if (texColor.a >= alphaCut){
				discard;
			}
		}else{
			discard;
		}
	}
	
	fragColor = texColor;
}
