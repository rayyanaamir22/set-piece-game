#version 410 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;  // the cascade's light view-projection
uniform mat4 uModel;
void main() { gl_Position = uViewProj * uModel * vec4(aPos, 1.0); }
