#version 410 core
#include "common.glsl"
#include "ibl.glsl"
in vec2 vUV;
uniform samplerCube uEnv;
uniform float uRough;
uniform float uEnvRes;
uniform vec3 uFwd;
uniform vec3 uRight;
uniform vec3 uVUp;
out vec4 oColor;
void main() {
  float u = vUV.x * 2.0 - 1.0;
  float v = vUV.y * 2.0 - 1.0;
  vec3 N = normalize(uFwd + u * uRight + v * uVUp);
  vec3 V = N;
  const uint SAMPLES = 96u;
  vec3 sum = vec3(0.0);
  float wsum = 0.0;
  for (uint i = 0u; i < SAMPLES; ++i) {
    vec2 Xi = hammersley(i, SAMPLES);
    vec3 H = importanceSampleGGX(Xi, N, uRough);
    vec3 L = normalize(2.0 * dot(V, H) * H - V);
    float NdotL = dot(N, L);
    if (NdotL > 0.0) {
      float NdotH = max(dot(N, H), 0.0);
      float HdotV = max(dot(H, V), 0.0);
      float pdf = distGGX(NdotH, uRough) * NdotH / (4.0 * HdotV) + 1e-4;
      float saTexel = 4.0 * PI / (6.0 * uEnvRes * uEnvRes);
      float saSample = 1.0 / (float(SAMPLES) * pdf + 1e-4);
      float mip = uRough == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);
      sum += textureLod(uEnv, L, mip).rgb * NdotL;
      wsum += NdotL;
    }
  }
  oColor = vec4(sum / max(wsum, 1e-4), 1.0);
}
