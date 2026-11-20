float radicalInverse(uint bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10;
}
vec2 hammersley(uint i, uint n) { return vec2(float(i) / float(n), radicalInverse(i)); }
vec3 importanceSampleGGX(vec2 Xi, vec3 N, float rough) {
  float a = rough * rough;
  float phi = 2.0 * PI * Xi.x;
  float cosT = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
  float sinT = sqrt(1.0 - cosT * cosT);
  vec3 H = vec3(cos(phi) * sinT, sin(phi) * sinT, cosT);
  vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
  vec3 T = normalize(cross(up, N));
  vec3 B = cross(N, T);
  return normalize(T * H.x + B * H.y + N * H.z);
}
float distGGX(float NdotH, float rough) {
  float a = rough * rough, a2 = a * a;
  float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / (PI * d * d);
}
