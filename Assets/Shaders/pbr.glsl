// PBR Reference
// https://google.github.io/filament/Filament.md.html
// https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
// learnopengl.com
#extension GL_GOOGLE_include_directive: require
#include "material.glsl"

const float PI = 3.1415926f;
const uint INVALID_TEXTURE = 0xFFFFFFFF;

// Normal Distribution Function
// Approximates distribution of microfacet
float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

// Geometric Function models the visibility (or occlusion or shadow masking) of microfacets
// Smith Geometric Shadowing Function
// This is derived as G / (4 * (n.v) * (n.l)) from original equation. So we don't need to
// divide by the denominator while combining it all
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.00001f);
}

float V_SmithGGX(float NoV, float NoL, float roughness) {

    float ggx2 = GeometrySchlickGGX(NoV, roughness);
    float ggx1 = GeometrySchlickGGX(NoL, roughness);
    return ggx1 * ggx2;
}

// Fresnel Schilick
vec3 F_Schlick(float LoH, vec3 f0) {
    return f0 + (1.0f - f0) * pow(1.0 - LoH, 5.0);
}


vec3 F_SchlickRoughness(float cosTheta, vec3 f0, float roughness)
{
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 

// https://learnopengl.com/PBR/IBL/Specular-IBL
// https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}  

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 up = abs(N.z) < 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

#ifdef FRAGMENT_SHADER
void GetMetallicRoughness(sampler2D metallicMap, vec2 uv, inout float metallic, inout float roughness)
{
	vec4 metallicRoughness = texture(metallicMap, uv);
	metallic = metallicRoughness.b;
    roughness = metallicRoughness.g;
}

vec3 GetNormalFromNormalMap(uint normalMapIndex, mat3 tbn)
{
    vec3 normal = texture(uTextures[nonuniformEXT(normalMapIndex)], fs_in.uv).rgb * 2.0f - 1.0f;
	return normalize(tbn * normal);
}


const vec3 cascadeColor[4] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0)
);

const float globalAOMultiplier = 0.5f;
#define CASCADE_DEBUG_COLOR 0

vec3 CalculateColor(in Material material)
{
	vec4 albedo = material.albedo;

	if(material.albedoMap != INVALID_TEXTURE)
       albedo = texture(uTextures[nonuniformEXT(material.albedoMap)], fs_in.uv);

	if(albedo.a < 0.5f)
	  discard;

	//albedo.rgb = pow(albedo.rgb, vec3(2.2f));

	float roughness = material.roughness;
	float ao = material.ao * globalAOMultiplier;
	float metallic = material.metallic;

	if(material.metallicMap != INVALID_TEXTURE)
	{
	   if(material.metallicMap == material.roughnessMap)
    	   GetMetallicRoughness(uTextures[nonuniformEXT(material.metallicMap)], fs_in.uv, metallic, roughness);
		else {
           metallic = texture(uTextures[nonuniformEXT(material.metallicMap)], fs_in.uv).r;
		   roughness = texture(uTextures[nonuniformEXT(material.roughnessMap)], fs_in.uv).r;
		}
	}

	if(material.ambientOcclusionMap != INVALID_TEXTURE)
       ao = texture(uTextures[nonuniformEXT(material.ambientOcclusionMap)], fs_in.uv).r * globalAOMultiplier;

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
	   emissive = texture(uTextures[nonuniformEXT(material.emissiveMap)], fs_in.uv).rgb * material.emissive;

	Lo += ambient + emissive;

	if(globals.enableCascadeDebug >	0)
       Lo *= cascadeColor[cascadeIndex] * 0.5f;


	return Lo;
}
#endif