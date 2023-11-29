#version 450

#extension GL_GOOGLE_include_directive : require 

#include "bindless.glsl"

layout(push_constant) uniform PushConstants {
   mat4 uProjectionMatrix;
   mat4 uNormalViewMatrix;
   uint uDepthMap;
   uint uNormalMap;
   vec2 uNoiseScale;

   int uKernelSamples;
   float uKernelRadius;
   float uBias;
   float uWSNormal;
};

layout(binding = 0) uniform readonly KernelBuffer{
   vec4 kernelVectors[64];
};

layout(binding = 1) uniform sampler2D uRandomTexture;

layout(location = 0) out float fragColor;
layout(location = 0) in vec2 uv;

// Convert Screen Space Position to View Space Position
vec3 GetViewSpacePos(vec2 uv) {
    float depth = texture(uTextures[nonuniformEXT(uDepthMap)], uv).r;
    vec4 sspos = vec4(uv, depth, 1.0f);
    sspos.xyz = sspos.xyz * 2.0f - 1.0f;
    vec4 viewPos = inverse(uProjectionMatrix) * sspos;
    return viewPos.xyz / viewPos.w;
}

void main() {
   vec3 viewPos = GetViewSpacePos(uv);
   vec3 normal = texture(uTextures[nonuniformEXT(uNormalMap)], uv).rgb;
   if(uWSNormal < 0.5f)
     normal = vec3(uNormalViewMatrix * vec4(normal, 0.0));

   vec3 randomVec = vec3(texture(uRandomTexture, uv * uNoiseScale).xy, 0.0f);
   vec3 t = normalize(randomVec - normal * dot(randomVec, normal));
   vec3 bt = cross(normal, t);
   mat3 tbn = mat3(t, bt, normal);

   float ao = 0.0;
   for(int i = 0; i < uKernelSamples; ++i) {
      vec3 direction = tbn * kernelVectors[i].xyz;

      // ViewSpace sample position
      vec3 samplePos = viewPos + direction * uKernelRadius;

      vec4 offset = uProjectionMatrix * vec4(samplePos, 1.0f);
      offset.xyz /= offset.w;
      offset.xyz = offset.xyz * 0.5 + 0.5;

      // View Space Depth
      float sampleDepth = GetViewSpacePos(offset.xy).z;

      // ViewSpace depth position
      float rangeCheck = smoothstep(0.0, 1.0, uKernelRadius / abs(viewPos.z - sampleDepth));
      ao += ((sampleDepth >= samplePos.z + uBias) ? 1.0f : 0.0f) * rangeCheck;
   }

   fragColor = 1.0 - ao / float(uKernelSamples);
}