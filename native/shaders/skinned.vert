#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aMat;
layout(location = 3) in ivec4 aJoints;
layout(location = 4) in vec4 aWeights;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform mat4 uBones[24];   // model-space skinning matrices (world pose * inverse bind)
out vec3 vN;
out vec3 vWorld;
flat out float vMat;
void main() {
  mat4 skin = aWeights.x * uBones[aJoints.x] + aWeights.y * uBones[aJoints.y] + aWeights.z * uBones[aJoints.z] + aWeights.w * uBones[aJoints.w];
  vec4 w = uModel * (skin * vec4(aPos, 1.0));
  vWorld = w.xyz;
  vN = normalize(mat3(uModel) * mat3(skin) * aNormal);
  vMat = aMat;
  gl_Position = uViewProj * w;
}
