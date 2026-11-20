#version 410 core
#include "common.glsl"
in vec2 vUV;
uniform samplerCube uEnv;
uniform vec3 uFwd;
uniform vec3 uRight;
uniform vec3 uVUp;
out vec4 oColor;
void main() {
  float u = vUV.x * 2.0 - 1.0;
  float v = vUV.y * 2.0 - 1.0;
  vec3 N = normalize(uFwd + u * uRight + v * uVUp);
  vec3 up = abs(N.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
  vec3 right = normalize(cross(up, N));
  up = cross(N, right);
  // Cosine-weighted average radiance over the hemisphere (this is irradiance / PI).
  vec3 sum = vec3(0.0);
  float n = 0.0;
  const float delta = 0.06;
  for (float phi = 0.0; phi < 2.0 * PI; phi += delta) {
    for (float th = 0.0; th < 0.5 * PI; th += delta) {
      vec3 t = vec3(sin(th) * cos(phi), sin(th) * sin(phi), cos(th));
      vec3 s = t.x * right + t.y * up + t.z * N;
      sum += textureLod(uEnv, s, 3.0).rgb * cos(th) * sin(th);
      n += 1.0;
    }
  }
  oColor = vec4(sum * PI / n, 1.0);  // = E / PI: a uniform sky of radiance L returns L
}
