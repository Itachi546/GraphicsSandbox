#version 450

#extension GL_GOOGLE_include_directive: require

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

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

const uint INVALID_TEXTURE = 0xFFFFFFFF;
const float globalAOMultiplier = 0.5f;
struct Material
{
	vec4 albedo;
	float emissive;
	float roughness;
	float metallic;
	float ao;
	float transparency;

	uint albedoMap;
	uint normalMap;
	uint emissiveMap;
	uint metallicMap;
	uint roughnessMap;
	uint ambientOcclusionMap;
	uint opacityMap;
};

#define FRAGMENT_SHADER
#include "pbr.glsl"
#include "bindings.glsl"
#include "shadow.glsl"

#define CASCADE_DEBUG_COLOR 0

void GetMetallicRoughness(uint metallicMapIndex, uint roughnessMapIndex, inout float metallic, inout float roughness)
{
	if(metallicMapIndex != INVALID_TEXTURE)
	{
	    vec4 metallicRoughness = texture(uTextures[metallicMapIndex], fs_in.uv);
    	if(roughnessMapIndex == metallicMapIndex)
		{
			   metallic = metallicRoughness.b;
			   roughness = metallicRoughness.g;
		}
	    else 
		{
           metallic = metallicRoughness.r;
		   vec4 roughnessVal = texture(uTextures[roughnessMapIndex], fs_in.uv);
		   roughness = roughnessVal.r;
		}
	}
}

vec3 GetNormalFromNormalMap(uint normalMapIndex, mat3 tbn)
{
    vec3 normal = texture(uTextures[normalMapIndex], fs_in.uv).rgb * 2.0f - 1.0f;
	return normalize(tbn * normal);
}

vec3 cascadeColor[4] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0)
);

void main()
{
	Material material = aMaterialData[fs_in.matId];
	vec4 albedo = material.albedo;

	if(material.albedoMap != INVALID_TEXTURE)
       albedo = texture(uTextures[material.albedoMap], fs_in.uv);

	if(albedo.a < 0.5f)
	  discard;

	//albedo.rgb = pow(albedo.rgb, vec3(2.2f));

	float roughness = material.roughness;
	float ao = material.ao * globalAOMultiplier;
	float metallic = material.metallic;
	GetMetallicRoughness(material.metallicMap, material.roughnessMap, metallic, roughness);

	if(material.ambientOcclusionMap != INVALID_TEXTURE)
       ao = texture(uTextures[material.ambientOcclusionMap], fs_in.uv).r * globalAOMultiplier;

    vec3 n = normalize(fs_in.normal);
	if(globals.enableNormalMapping > 0)
	{
    	vec3 t = normalize(fs_in.tangent);
	    vec3 bt = normalize(fs_in.bitangent);
		mat3 tbn = mat3(t, bt, n);
		if(material.normalMap != INVALID_TEXTURE)
		n =	GetNormalFromNormalMap(material.normalMap, tbn);
	}
    vec3 v = normalize(fs_in.viewDir);
	vec3 r = reflect(-v, n);

	float NoV =	max(dot(n, v), 0.0001);
	vec3 Lo = vec3(0.0f);

	vec3 f0	= mix(vec3(0.04), albedo.rgb, metallic);

	// Calculate shadow factor
	

	int nLight = globals.nLight;
	int cascadeIndex = 0;
	for(int i = 0; i < nLight; ++i)
	{
	    LightData light = globals.lights[i];
		//Light light = light[i];
    	vec3 l = light.position;
		float attenuation = 1.0f;
		float shadow = 1.0f;
		if(light.type > 0.2f)
		{
            vec3 l = light.position - fs_in.worldPos;
		    float dist = length(l);
			attenuation	= 1.0f / (dist * dist);
     		l /= dist;
        }
		else 
		{
    		l= normalize(l);
			shadow = CalculateShadowFactor(fs_in.worldPos, abs(fs_in.lsPos.z), cascadeIndex);
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

    if(globals.enableCascadeDebug > 0)
       Lo *= cascadeColor[cascadeIndex] * 0.5f;

	fragColor =	vec4(Lo, 1.0f);
}