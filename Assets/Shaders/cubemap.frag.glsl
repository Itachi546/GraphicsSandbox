#version 450

layout(location = 0) out vec4 fragColor;
layout(binding = 2) uniform samplerCube uCubemap;

layout(location = 0) in vec3 uv;

void main()
{
   vec3 col = texture(uCubemap, uv).rgb;
   col /= (1.0f + col);
   col = pow(col, vec3(0.4545));
   fragColor = vec4(col, 1.0f);
}