// rgba_v? Matches:
// src/emulator/shaders/unknown/v/0b6136ca31968ab70dd3832f49cd4f7341da6f58e380f42f5019abd0939b5e00.vert (this)
// src/emulator/shaders/unknown/v/87d9dcd0e912e1fdf17d374b029d7f175f6643b7be079cca5c32b902d80df232.vert
// src/emulator/shaders/unknown/v/b5fe8ae98b04e92ae6fb356910b3297816b2e622f852c4279016d6c4218a3c1d.vert
#version 120

attribute vec3 aPosition;
attribute vec4 aColor;
uniform mat4 wvp;

varying vec4 vColor;

void main() {
    gl_Position = vec4(aPosition, 1) * wvp;
    vColor = aColor;
}
