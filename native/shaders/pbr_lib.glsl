// Shared PBR lighting: Cook-Torrance GGX sun with cascaded shadows, plus image-based ambient.
// The including fragment shader declares vN, vWorld and calls shadePbr().
uniform vec3 uCam;
uniform vec3 uCamFwd;
uniform vec3 uSunDir;
uniform vec3 uSunE;        // linear sun irradiance (colour * intensity)
uniform float uIblScale;
uniform samplerCube uIrr;
uniform samplerCube uPref;
uniform sampler2D uBrdf;
uniform float uPrefMaxLod;
uniform vec3 uFogColor;
uniform vec2 uFog;
uniform int uShadowOn;
uniform int uPcf;
uniform sampler2DArrayShadow uShadow;
uniform mat4 uShadowMat[3];
uniform vec3 uSplits;      // far view-depth of each cascade
uniform vec3 uTexel;       // world size of a shadow texel per cascade

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

float shadowFactor(vec3 N) {
  if (uShadowOn == 0) return 1.0;
  float depth = dot(vWorld - uCam, uCamFwd);
  int c = depth < uSplits.x ? 0 : (depth < uSplits.y ? 1 : 2);
  if (depth >= uSplits.z) return 1.0;
  float nl = clamp(dot(N, uSunDir), 0.0, 1.0);
  vec3 p = vWorld + N * uTexel[c] * (1.5 + 2.0 * (1.0 - nl));  // normal-offset bias
  vec4 lp = uShadowMat[c] * vec4(p, 1.0);
  vec3 sc = lp.xyz * 0.5 + 0.5;
  if (sc.x < 0.0 || sc.x > 1.0 || sc.y < 0.0 || sc.y > 1.0 || sc.z > 1.0) return 1.0;
  float texel = 1.0 / float(textureSize(uShadow, 0).x);
  float sum = 0.0;
  int r = uPcf;
  for (int y = -r; y <= r; ++y)
    for (int x = -r; x <= r; ++x)
      sum += texture(uShadow, vec4(sc.xy + vec2(x, y) * texel, float(c), sc.z));
  float s = sum / float((2 * r + 1) * (2 * r + 1));
  // Fade out at the far end of the last cascade.
  float fade = smoothstep(uSplits.z * 0.85, uSplits.z, depth);
  return mix(s, 1.0, fade);
}

float D_GGX(float NdotH, float rough) {
  float a = rough * rough, a2 = a * a;
  float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
  return a2 / (PI * d * d);
}
float G_Smith(float NdotV, float NdotL, float rough) {
  float k = (rough + 1.0) * (rough + 1.0) / 8.0;
  return (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));
}
vec3 fresnel(float cosT, vec3 F0) { return F0 + (1.0 - F0) * pow(1.0 - cosT, 5.0); }
vec3 fresnelRough(float cosT, vec3 F0, float rough) { return F0 + (max(vec3(1.0 - rough), F0) - F0) * pow(1.0 - cosT, 5.0); }

vec4 shadePbr(vec3 albedo, float alpha, float roughIn, float metal) {
  vec3 N = normalize(vN);
  vec3 V = normalize(uCam - vWorld);
  float rough = clamp(roughIn, 0.045, 1.0);
  float NdotV = max(dot(N, V), 1e-4);
  vec3 F0 = mix(vec3(0.04), albedo, metal);

  // Sun: Cook-Torrance GGX with cascaded shadow maps.
  vec3 L = normalize(uSunDir);
  float NdotL = max(dot(N, L), 0.0);
  vec3 Lo = vec3(0.0);
  if (NdotL > 0.0) {
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    vec3 F = fresnel(VdotH, F0);
    vec3 spec = D_GGX(NdotH, rough) * G_Smith(NdotV, NdotL, rough) * F / (4.0 * NdotV * NdotL + 1e-4);
    vec3 kD = (1.0 - F) * (1.0 - metal);
    Lo = (kD * albedo / PI + spec) * uSunE * NdotL * shadowFactor(N);
  }

  // Sky: image-based lighting (the irradiance map stores irradiance / PI).
  vec3 F = fresnelRough(NdotV, F0, rough);
  vec3 kD = (1.0 - F) * (1.0 - metal);
  vec3 diffuse = texture(uIrr, N).rgb * albedo;
  vec3 R = reflect(-V, N);
  vec3 pref = textureLod(uPref, R, rough * uPrefMaxLod).rgb;
  vec2 brdf = texture(uBrdf, vec2(NdotV, rough)).rg;
  vec3 spec = pref * (F * brdf.x + brdf.y);
  vec3 ambient = (kD * diffuse + spec) * uIblScale;

  vec3 col = Lo + ambient;
  float dist = length(vWorld - uCam);
  float f = clamp((dist - uFog.x) / (uFog.y - uFog.x), 0.0, 1.0);
  col = mix(col, uFogColor, f);
  return vec4(col, alpha);
}
