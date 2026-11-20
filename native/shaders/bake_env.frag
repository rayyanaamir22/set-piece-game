#version 410 core
#include "common.glsl"
#include "sky.glsl"
in vec2 vUV;
uniform vec3 uFwd;
uniform vec3 uRight;
uniform vec3 uVUp;  // direction the face's v coordinate moves along
out vec4 oColor;
void main() {
  float u = vUV.x * 2.0 - 1.0;
  float v = vUV.y * 2.0 - 1.0;
  vec3 d = normalize(uFwd + u * uRight + v * uVUp);
  oColor = vec4(skyBase(d), 1.0);
}
