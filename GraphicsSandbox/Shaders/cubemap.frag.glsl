#version 450

#extension GL_GOOGLE_include_directive: require
#include "utils.glsl"

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

layout(binding = 2) uniform samplerCube uCubemap;

layout(location = 0) in vec3 uv;

layout(push_constant) uniform PushConstants {
  float bloomThreshold;
};

void main()
{
   vec3 col = texture(uCubemap, uv).rgb;
   fragColor = vec4(col, 1.0f);
   if(rgb2Luma(col) > bloomThreshold) 
     brightColor = fragColor;
   else 
    brightColor = vec4(0.0f);
}