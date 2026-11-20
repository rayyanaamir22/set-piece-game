#version 410 core
#include "common.glsl"
in vec3 vN;
in vec3 vWorld;
flat in float vMat;
uniform vec3 uPalette[6];     // sRGB colour per material slot
uniform float uPaletteRough[6];
out vec4 oColor;
#include "pbr_lib.glsl"
void main() {
  int m = int(vMat + 0.5);
  oColor = shadePbr(toLin(uPalette[m]), 1.0, uPaletteRough[m], 0.0);
}
