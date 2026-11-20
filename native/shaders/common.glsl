const float PI = 3.14159265359;
vec3 toLin(vec3 c) { return pow(c, vec3(2.2)); }
vec3 srgbOetf(vec3 c) {
  c = clamp(c, 0.0, 1.0);
  return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}
