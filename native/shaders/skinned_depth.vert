#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 3) in ivec4 aJoints;
layout(location = 4) in vec4 aWeights;
uniform mat4 uViewProj;  // the cascade's light view-projection
uniform mat4 uModel;
uniform mat4 uBones[24];
void main() {
  mat4 skin = aWeights.x * uBones[aJoints.x] + aWeights.y * uBones[aJoints.y] + aWeights.z * uBones[aJoints.z] + aWeights.w * uBones[aJoints.w];
  gl_Position = uViewProj * uModel * (skin * vec4(aPos, 1.0));
}
