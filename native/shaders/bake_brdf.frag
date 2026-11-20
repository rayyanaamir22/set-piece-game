#version 410 core
#include "common.glsl"
#include "ibl.glsl"
in vec2 vUV;
out vec4 oColor;
float geomSchlick(float NdotV, float rough) {
  float k = rough * rough / 2.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}
void main() {
  float NdotV = max(vUV.x, 1e-3);
  float rough = max(vUV.y, 0.02);
  vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
  vec3 N = vec3(0.0, 0.0, 1.0);
  float A = 0.0, B = 0.0;
  const uint SAMPLES = 256u;
  for (uint i = 0u; i < SAMPLES; ++i) {
    vec2 Xi = hammersley(i, SAMPLES);
    vec3 H = importanceSampleGGX(Xi, N, rough);
    vec3 L = normalize(2.0 * dot(V, H) * H - V);
    float NdotL = max(L.z, 0.0), NdotH = max(H.z, 0.0), VdotH = max(dot(V, H), 0.0);
    if (NdotL > 0.0) {
      float G = geomSchlick(NdotV, rough) * geomSchlick(NdotL, rough);
      float Gv = G * VdotH / (NdotH * NdotV);
      float Fc = pow(1.0 - VdotH, 5.0);
      A += (1.0 - Fc) * Gv;
      B += Fc * Gv;
    }
  }
  oColor = vec4(A / float(SAMPLES), B / float(SAMPLES), 0.0, 1.0);
}
