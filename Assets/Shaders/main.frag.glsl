#version 450

#extension GL_GOOGLE_include_directive: require

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

layout(location = 0) in VS_OUT 
{
   vec3 normal;
   vec3 worldPos;
   vec3 viewDir;
   flat uint matId;
} fs_in;

struct Material
{
   vec4 albedo;
   float emissive;
   float roughness;
   float metallic;
   float ao;
};

#define FRAGMENT_SHADER
#include "pbr.glsl"
#include "bindings.glsl"

void main()
{
	vec2 uv = fs_in.worldPos.xz;
	float checker = mod(floor(uv.x) + floor(uv.y), 2.0f);

	Material material = aMaterialData[fs_in.matId];
	vec4 albedo = material.albedo;
/*
	if(material.emissive > 0.0f)
	{
	    brightColor = material.emissive * albedo;
		fragColor = material.emissive * albedo;
    	return;
	}
	*/
	float roughness = material.roughness;
	float ao = material.ao;
	float metallic = material.metallic;

    vec3 n = normalize(fs_in.normal);
    vec3 v = normalize(fs_in.viewDir);
	vec3 r = reflect(-v, n);

	float NoV =	max(dot(n, v), 0.0);
	vec3 Lo = vec3(0.0f);

	vec3 f0	= mix(vec3(0.04), albedo.rgb, metallic);

	int nLight = globals.nLight;
	for(int i = 0; i < nLight; ++i)
	{
	    LightData light = globals.lights[i];
		//Light light = light[i];
    	vec3 l = light.position - fs_in.worldPos;
		float dist = length(l);
		l /= dist;
    	vec3 h = normalize(v + l);

		float NoL =	clamp(dot(n, l), 0.0, 1.0);
		float LoH =	clamp(dot(l, h), 0.0, 1.0);
		float NoH =	clamp(dot(n, h), 0.0, 1.0);

		float D	= D_GGX(NoH, roughness * roughness);
		vec3  F	= F_Schlick(LoH, f0);
		float V	= V_SmithGGX(NoV, NoL, roughness);

		vec3  Ks = F;
		vec3  Kd = (1.0f - Ks) * (1.0f - metallic);
		vec3 specular =	D *	F *	V / (4.0f * NoV * NoL + 0.0001f);

		float attenuation = 1.0f;
		if(light.type > 0.2f) // Other than directional light
		     attenuation = 1.0f / (dist * dist);
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
	Lo += ambient + material.emissive * albedo.rgb;

    float luminance = dot(Lo, vec3(0.2126, 0.7152, 0.0722));
	if(luminance > globals.bloomThreshold || material.emissive > 0.01f)
     	brightColor = vec4(Lo, 1.0f);
	else 
	    brightColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	//Lo /=(1.0f + Lo);
    //Lo = pow(Lo, vec3(0.4545));
	fragColor =	vec4(Lo, 1.0f);
}