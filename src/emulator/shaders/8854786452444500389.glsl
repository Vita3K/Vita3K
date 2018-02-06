// https://github.com/xerpi/libvita2d/blob/master/libvita2d/shader/color_v.cg
#version 120

uniform mat4 wvp;
attribute vec3 aPosition;
attribute vec4 aColor;

varying vec4 vColor;

mat4 transpose(mat4 m) {
  return mat4(m[0][0], m[1][0], m[2][0], m[3][0],
              m[0][1], m[1][1], m[2][1], m[3][1],
              m[0][2], m[1][2], m[2][2], m[3][2],
              m[0][3], m[1][3], m[2][3], m[3][3]);
}

void main()
{
    gl_Position = vec4(aPosition, 1) * transpose(wvp);
    gl_Position.y = -gl_Position.y;
    vColor = aColor;
}
