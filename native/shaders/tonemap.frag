#version 410 core
#include "common.glsl"
in vec2 vUV;
uniform sampler2D uHdr;
uniform float uExposure;
out vec4 oColor;
// Narkowicz's fit of the ACES filmic curve.
vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }
void main() {
  vec3 c = texture(uHdr, vUV).rgb * uExposure;
  oColor = vec4(srgbOetf(aces(c)), 1.0);
}
