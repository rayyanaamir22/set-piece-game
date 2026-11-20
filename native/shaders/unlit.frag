#version 410 core
in vec3 vN;
in vec3 vWorld;
in vec3 vObj;
uniform vec4 uColor;
out vec4 oColor;
void main() { oColor = uColor; }
