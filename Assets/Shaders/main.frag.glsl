#version 450

#extension GL_GOOGLE_include_directive: require

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

layout(location = 0) in VS_OUT 
{
   vec3 normal;
   vec3 worldPos;
   vec3 viewDir;
   vec2 uv;
   flat uint matId;
} fs_in;

const uint INVALID_TEXTURE = 0xFFFFFFFF;

struct Material
{
	vec4 albedo;
	float emissive;
	float roughness;
	float metallic;
	float ao;
	float transparency;

	uint albedoMap;
	uint normalTexture;
	uint emissiveMap;
	uint metallicMap;
	uint roughnessMap;
	uint ambientOcclusionMap;
	uint opacityMap;
};

#define FRAGMENT_SHADER
#include "pbr.glsl"
#include "bindings.glsl"

void GetMetallicRoughness(uint metallicMapIndex, uint roughnessMapIndex, inout float metallic, inout float roughness)
{
	if(metallicMapIndex != INVALID_TEXTURE)
	{
	    vec4 metallicRoughness = texture(uTextures[metallicMapIndex], fs_in.uv);
    	if(roughnessMapIndex == INVALID_TEXTURE)
		{
			   roughness = metallicRoughness.g;
			   metallic = metallicRoughness.b;
		}
	    else 
		{
           metallic = metallicRoughness.r;
		   vec4 roughnessVal = texture(uTextures[roughnessMapIndex], fs_in.uv);
		   roughness = roughnessVal.r;
		}
	}
}

void main()
{
	vec2 uv = fs_in.worldPos.xz;
	Material material = aMaterialData[fs_in.matId];
	vec4 albedo = material.albedo;

	if(material.albedoMap != INVALID_TEXTURE)
       albedo = texture(uTextures[material.albedoMap], fs_in.uv);

	if(albedo.a < 0.5f)
	  discard;

	//albedo.rgb = pow(albedo.rgb, vec3(2.2f));

	float roughness = 0.9f;//material.roughness;
	float ao = material.ao * 0.5f;
	float metallic = 0.01f;//material.metallic;
	GetMetallicRoughness(material.metallicMap, material.roughnessMap, metallic, roughness);

	if(material.ambientOcclusionMap != INVALID_TEXTURE)
       ao = texture(uTextures[material.ambientOcclusionMap], fs_in.uv).r;

    vec3 n = normalize(fs_in.normal);
    vec3 v = normalize(fs_in.viewDir);
	vec3 r = reflect(-v, n);

	float NoV =	max(dot(n, v), 0.0001);
	vec3 Lo = vec3(0.0f);

	vec3 f0	= mix(vec3(0.04), albedo.rgb, metallic);

	int nLight = globals.nLight;
	for(int i = 0; i < nLight; ++i)
	{
	    LightData light = globals.lights[i];
		//Light light = light[i];
    	vec3 l = light.position;
		float attenuation = 1.0f;
		if(light.type > 0.2f)
		{
            light.position - fs_in.worldPos;
		    float dist = length(l);
			attenuation	= 1.0f / (dist * dist);
     		l /= dist;
        }
		else 
    		l= normalize(l);
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

		Lo	+= (Kd * (albedo.rgb / PI) + specular) * NoL * light.color * attenuation;
	}

	vec3 Ks = F_SchlickRoughness(NoV, f0, roughness);
	vec3 Kd = (1.0f - Ks) * (1.0f - metallic);
	vec3 irradiance = texture(uIrradianceMap, n).rgb;
	vec3 diffuse = irradiance * albedo.rgb;

	const float MAX_REFLECTION_LOD = 6.0;
	vec3 prefilteredColor = textureLod(uPrefilterEnvMap, r, roughness * MAX_REFLECTION_LOD).rgb;
	vec2 brdf = texture(uBRDFLUT, vec2(NoV, roughness)).rg;
	vec3 specular = prefilteredColor * (Ks * brdf.x + brdf.y);

	vec3 ambient = (Kd * diffuse + specular) * ao;
	vec3 emissive = material.emissive * albedo.rgb;

	if(material.emissiveMap != INVALID_TEXTURE)
	   emissive = texture(uTextures[material.emissiveMap], fs_in.uv).rgb * material.emissive;

	Lo += ambient + emissive;

    float luminance = dot(Lo, vec3(0.2126, 0.7152, 0.0722));
	if(luminance > globals.bloomThreshold || material.emissive > 0.01f)
	{
	    // Nasty inf pixel that become visible in bloom
		if(isinf(Lo.x))
		  brightColor = vec4(0.0f);
	    else
		   brightColor	= vec4(Lo, 1.0f);
	}
	else 
	    brightColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	fragColor =	vec4(Lo, 1.0f);
}