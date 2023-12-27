#version 450
#extension GL_GOOGLE_include_directive: require

#include "pbr.glsl"
#include "globaldata.glsl"
#include "material.glsl"

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

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

layout(binding = 4) readonly buffer MaterialData {
  Material aMaterialData[];
};

layout(binding = 5) readonly buffer LightBuffer {
   LightData lightData[];
} lightBuffer;

layout(binding = 6, std140) uniform CascadeInfo
{
   Cascade cascades[MAX_CASCADES];
   float shadowMapWidth;
   float shadowMapHeight;
   int uPCFRadius;
   float uPCFRadiusMultiplier;
};

#include "bindless.glsl"
#include "shadow.glsl"

layout(push_constant) uniform PushConstants
{
	uint nLight;
	uint uIrradianceMap;
	uint uPrefilterEnvMap;
	uint uBRDFLUT;

	vec3 uCameraPosition;
	float exposure;
	float globalAO;
	uint directionalShadowMap;
};

vec3 ACESFilm(vec3 x)
{
   float a = 2.51f;
   float b = 0.03f;
   float c = 2.43f;
   float d = 0.59f;
   float e = 0.14f;
   return clamp((x*(a*x+b))/(x*(c*x+d)+e), vec3(0.0f), vec3(1.0f));
}

vec3 getPertubedNormal(Material material, vec2 uv)
{
    vec3 bumpNormal = normalize(texture(uTextures[nonuniformEXT(material.normalMap)], uv).rgb * 2.0f - 1.0f);
    vec3 t = fs_in.tangent;
	vec3 bt	= fs_in.bitangent;
	vec3 n = fs_in.normal;
    mat3 tbn = mat3(t, bt, n);
    return tbn * bumpNormal;
}

vec2 getMetallicRoughness(Material material, vec2 uv)
{
  float metallic = material.metallic;
  float roughness = material.roughness;
  if(material.metallicMap != INVALID_TEXTURE)
  {
      vec3 metallicRoughness = texture(uTextures[nonuniformEXT(material.metallicMap)], uv).rgb;
      if(material.metallicMap == material.roughnessMap)
	  {
	    metallic =  metallicRoughness.b;
        roughness = metallicRoughness.g;
	  }
      else {
     	metallic =  metallicRoughness.r;
	    roughness = texture(uTextures[nonuniformEXT(material.roughnessMap)], uv).r;
 	}
  }
  return vec2(metallic, roughness);
}

vec4 CalculateColor(vec2 uv)
{
    Material material = aMaterialData[fs_in.matId];
  
    vec4 albedo = material.albedo;

    if(material.albedoMap != INVALID_TEXTURE)
       albedo *= texture(uTextures[nonuniformEXT(material.albedoMap)], uv);

	float alpha = albedo.a;
	if(material.alphaMode == ALPHAMODE_MASK && alpha < material.alphaCutoff)
	   discard;
	
	if(material.alphaMode == ALPHAMODE_BLEND)
	   alpha *= material.transparency;

    // Export normals
    vec3 n = fs_in.normal;
    if(material.normalMap != INVALID_TEXTURE)
      n = normalize(getPertubedNormal(material, uv));

    vec2 metallicRoughness = getMetallicRoughness(material, uv);
    float ao = material.ao;
    if(material.ambientOcclusionMap != INVALID_TEXTURE)
      ao = texture(uTextures[nonuniformEXT(material.ambientOcclusionMap)], fs_in.uv).r;

	ao *= globalAO;

	vec3 worldPos = fs_in.worldPos;
	
    vec3 v = normalize(uCameraPosition - worldPos);
	vec3 r = reflect(-v, n);

	float NoV =	max(dot(n, v), 0.0001);
	vec3 Lo = vec3(0.0f);

	float metallic = metallicRoughness.r;
	float roughness = metallicRoughness.g;
	vec3 f0	= mix(vec3(0.04), albedo.rgb, metallic);

	// Calculate shadow factor

	int cascadeIndex = 0;
	for(int i = 0; i < nLight; ++i)
	{
	    LightData light = lightBuffer.lightData[i];
		//Light light = light[i];
    	vec3 l = light.position;
		float attenuation = 1.0f;
		float shadow = 1.0f;
		if(light.type > 0.2f)
		{
            vec3 l = light.position - worldPos;
		    float dist = length(l);
			attenuation	= 1.0f / (dist * dist);
     		l /= dist;
        }
		else 
		{
    		l= normalize(l);
			shadow = CalculateShadowFactor(fs_in.worldPos, abs(fs_in.lsPos.z), directionalShadowMap, cascadeIndex);
		}

    	vec3 h = normalize(v + l);
		float NoL =	clamp(dot(n, l), 0.0, 1.0);
		float LoH =	clamp(dot(l, h), 0.0, 1.0);
		float NoH =	clamp(dot(n, h), 0.0, 1.0);

		float D	= D_GGX(NoH, roughness * roughness);
		vec3  F	= F_Schlick(LoH, f0);
		float V	= V_SmithGGX(NoV, NoL, roughness);

		vec3  Ks = F;
		vec3  Kd = (1.0f - Ks) * (1.0f - metallic);
		vec3 specular =	(V * F * D) / (4.0f * NoV * NoL + 0.0001f);

		Lo	+= (Kd * (albedo.rgb / PI) + specular) * NoL * light.color * attenuation * shadow;
	}

	vec3 Ks = F_SchlickRoughness(NoV, f0, roughness);
	vec3 Kd = (1.0f - Ks) * (1.0f - metallic);
	vec3 irradiance = texture(uTexturesCube[nonuniformEXT(uIrradianceMap)], n).rgb;
	vec3 diffuse = irradiance * albedo.rgb;

	const float MAX_REFLECTION_LOD = 6.0;
	vec3 prefilteredColor = textureLod(uTexturesCube[nonuniformEXT(uPrefilterEnvMap)], r, roughness * MAX_REFLECTION_LOD).rgb;
	vec2 brdf = texture(uTextures[nonuniformEXT(uBRDFLUT)], vec2(NoV, roughness)).rg;
	vec3 specular = prefilteredColor * (Ks * brdf.x + brdf.y);

	vec3 ambient = (Kd * diffuse + specular) * ao;
	vec3 emissive = material.emissive;

	if(material.emissiveMap != INVALID_TEXTURE)
	 emissive = texture(uTextures[nonuniformEXT(material.emissiveMap)], fs_in.uv).rgb * material.emissive;

	Lo += ambient + emissive;

	#if ENABLE_CASCADE_DEBUG
       Lo *= cascadeColor[cascadeIndex] * 0.5;
	#endif

	return vec4(Lo, alpha);
}

void main()
{
	vec4 Lo = CalculateColor(fs_in.uv);
	fragColor =	Lo;
	/*
 	Lo.xyz = ACESFilm(Lo.xyz * exposure);
    Lo.xyz = pow(Lo.xyz, vec3(0.4545));

	*/
}