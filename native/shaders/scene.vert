#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform mat3 uNormalMat;
out vec3 vN;
out vec3 vWorld;
out vec3 vObj;
void main() {
  vec4 w = uModel * vec4(aPos, 1.0);
  vWorld = w.xyz;
  vN = uNormalMat * aNormal;
  vObj = aPos;
  gl_Position = uViewProj * w;
}
