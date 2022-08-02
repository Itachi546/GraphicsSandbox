#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 vNormal;

void main()
{
   fragColor = vec4(vNormal * 0.5 + 0.5, 1.0f);
}