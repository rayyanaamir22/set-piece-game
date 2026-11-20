#version 410 core
#include "common.glsl"
in vec3 vN;
in vec3 vWorld;
in vec3 vObj;
uniform vec4 uColor;       // sRGB albedo + alpha
uniform float uRough;
uniform float uMetal;
uniform int uPattern;      // 1 = ball panels
uniform int uGrass;        // 1 = procedural turf variation
out vec4 oColor;
#include "pbr_lib.glsl"
void main() {
  vec3 albedo = toLin(uColor.rgb);
  if (uPattern == 1) {
    // Ball panels: a patch pattern in object space, so spin is readable.
    vec3 d = normalize(vObj);
    float p = sin(5.0 * d.x + 0.7) * sin(5.0 * d.y + 1.9) * sin(5.0 * d.z + 0.3);
    albedo = mix(albedo, toLin(vec3(0.10)), step(0.0, p));
  }
  if (uGrass == 1) {
    float n = vnoise(vWorld.xz * 0.6) * 0.5 + vnoise(vWorld.xz * 3.1) * 0.3 + vnoise(vWorld.xz * 17.0) * 0.2;
    albedo *= 0.82 + 0.36 * n;
  }
  oColor = shadePbr(albedo, uColor.a, uRough, uMetal);
}
