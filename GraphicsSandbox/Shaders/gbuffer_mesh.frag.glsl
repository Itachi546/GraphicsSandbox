#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier : enable

#include "material.glsl"
#include "constants.glsl"

layout(location = 0) out vec4 colorBuffer;
layout(location = 1) out vec4 normalBuffer;
layout(location = 2) out vec4 pbrBuffer;
layout(location = 3) out vec4 positionBuffer;
layout(location = 4) out vec4 emissiveBuffer;

layout(location = 0) in VS_OUT 
{
   vec3 normal;
   vec3 tangent;
   vec3 bitangent;
   vec3 worldPos;
   vec3 lsPos;
   vec3 viewDir;
   vec2 uv;
   flat uint matId;
} fs_in;

layout(binding = 3) readonly buffer MaterialData
{
    Material aMaterialData[];
};

void main()
{
  // Export positions
  positionBuffer = vec4(fs_in.worldPos, 1.0f);

  // Export colors
  colorBuffer =	vec4(fs_in.bitangent, 1.0f);

  // Export normals
  normalBuffer = vec4(normalize(fs_in.normal), 1.0f);

  // Export metallic, roughness and AO component
  pbrBuffer = vec4(1.0f);

  vec3 emissive = vec3(1.0f);
  emissiveBuffer = vec4(emissive, 1.0);
}
