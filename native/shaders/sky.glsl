// Procedural HDR sky shared by the background pass and the IBL bake.
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform vec3 uSkyGround;
uniform vec3 uSunDir;       // towards the sun
uniform vec3 uSunRadiance;  // linear colour of the sun disc and its glow
uniform float uSunCos;      // cosine of the sun's angular radius
uniform float uSunGlow;

// Sky without the sun disc (this is what the IBL integrates: the sun is a separate analytic light).
vec3 skyBase(vec3 d) {
  float h = d.y;
  vec3 up = mix(uSkyHorizon, uSkyZenith, pow(clamp(h, 0.0, 1.0), 0.45));
  vec3 down = mix(uSkyHorizon, uSkyGround, clamp(-h * 5.0, 0.0, 1.0));
  vec3 c = h >= 0.0 ? up : down;
  float s = max(dot(d, uSunDir), 0.0);
  // Aureole around the sun and a warm band along the horizon on that side.
  float horizonBand = exp(-abs(h) * 6.0);
  c += uSunRadiance * uSunGlow * (0.15 * pow(s, 8.0) + 0.6 * pow(s, 64.0) + 0.25 * horizonBand * pow(s, 2.0));
  return c;
}
vec3 skyDisc(vec3 d) {
  float cs = dot(d, uSunDir);
  float edge = smoothstep(uSunCos - 0.0003, uSunCos + 0.00015, cs);
  return uSunRadiance * 40.0 * edge * step(0.0, d.y + 0.02);
}
