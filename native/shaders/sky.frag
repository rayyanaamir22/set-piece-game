#version 410 core
#include "common.glsl"
#include "sky.glsl"
in vec2 vUV;
uniform mat4 uInvViewProj;
uniform vec3 uCam;
out vec4 oColor;
void main() {
  vec4 w = uInvViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
  vec3 d = normalize(w.xyz / w.w - uCam);
  oColor = vec4(skyBase(d) + skyDisc(d), 1.0);
}
