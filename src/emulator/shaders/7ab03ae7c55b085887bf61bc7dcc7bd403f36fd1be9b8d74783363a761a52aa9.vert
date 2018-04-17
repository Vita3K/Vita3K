// Vertex shader.
#version 120

attribute vec2 aPosition;
attribute vec2 aTexcoord;
attribute vec4 aColor;
attribute float aAngle;
attribute vec2 aTranslation;
uniform mat4 pm;
uniform float trsEnable;

varying vec4 vColor;
varying vec2 vTexcoord;

void main() {
	if (trsEnable > 0.5f){
		mat3 rotate_mat = mat3
			(cos(aAngle), sin(aAngle), 0.0f,
			-sin(aAngle), cos(aAngle), 0.0f,
			        0.0f,        0.0f, 1.0f);
			
		mat4 translate_mat = mat4
			(1.0f,           0.0f,           0.0f, 0.0f,
			 0.0f,           1.0f,           0.0f, 0.0f,
			 0.0f,           0.0f,           1.0f, 0.0f,
			 aTranslation.x, aTranslation.y, 0.0f, 1.0f);

		vec3 xformed = vec3(aPosition, 0.0f) * rotate_mat;

		gl_Position = (pm * translate_mat) * vec4(xformed, 1.0f);
	}else{
		gl_Position = pm * vec4(aPosition, 0.0f, 1.0f);
	}
	vColor = aColor;
	vTexcoord = aTexcoord;
}
