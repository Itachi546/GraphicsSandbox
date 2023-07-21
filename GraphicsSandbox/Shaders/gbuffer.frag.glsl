#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier : enable

#include "material.glsl"
#include "bindless.glsl"
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

layout(binding = 4) readonly buffer MaterialData
{
    Material aMaterialData[];
};

void main()
{
  Material material = aMaterialData[fs_in.matId];

  // Export positions
  positionBuffer = vec4(fs_in.worldPos, 1.0f);

  // Export colors
  vec4 albedo = material.albedo;
  if(material.albedoMap != INVALID_TEXTURE)
    albedo *= texture(uTextures[nonuniformEXT(material.albedoMap)], fs_in.uv);
  colorBuffer =	albedo;
  //colorBuffer =	vec4(fs_in.bitangent, 1.0);

  // Export normals
  vec3 n = fs_in.normal;
  if(material.normalMap != INVALID_TEXTURE)
  {    
    vec3 bumpNormal = normalize(texture(uTextures[nonuniformEXT(material.normalMap)], fs_in.uv).rgb * 2.0f - 1.0f);
	vec3 t = fs_in.tangent;
	vec3 bt	= fs_in.bitangent;
    mat3 tbn = mat3(t, bt, n);
    n = tbn * bumpNormal;
  }
  normalBuffer = vec4(normalize(n), 1.0f);

  // Export metallic, roughness and AO component
  float metallic = material.metallic;
  float roughness = material.roughness;
  if(material.metallicMap != INVALID_TEXTURE)
    {
      vec3 metallicRoughness = texture(uTextures[nonuniformEXT(material.metallicMap)], fs_in.uv).rgb;
      if(material.metallicMap == material.roughnessMap)
	{
	  metallic =  metallicRoughness.b;
	  roughness = metallicRoughness.g;
	}
      else {
	metallic =  metallicRoughness.r;
	roughness = texture(uTextures[nonuniformEXT(material.roughnessMap)], fs_in.uv).r;
      }
    }

  float ao = material.ao;
  if(material.ambientOcclusionMap != INVALID_TEXTURE)
    ao = texture(uTextures[nonuniformEXT(material.ambientOcclusionMap)], fs_in.uv).r;
  pbrBuffer = vec4(metallic, roughness, ao, 1.0);

  vec3 emissive = material.emissive;
  if(material.emissiveMap != INVALID_TEXTURE)
    emissive = texture(uTextures[nonuniformEXT(material.emissiveMap)], fs_in.uv).rgb;
  emissiveBuffer = vec4(emissive, 1.0);
}
