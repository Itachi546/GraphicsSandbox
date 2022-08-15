#version 450

#extension GL_GOOGLE_include_directive: require

layout(location = 0) out vec4 fragColor;

layout(location = 0) in VS_OUT 
{
   vec3 normal;
   vec3 worldPos;
   vec3 viewDir;
   float dt;
   flat uint matId;
} fs_in;

struct Material
{
   vec4 albedo;
   float roughness;
   float metallic;
   float ao;
   float unused_;

};

#define FRAGMENT_SHADER
#include "pbr.glsl"
#include "bindings.glsl"

const vec3 viewPos = vec3(0.0, 3.0, 5.0);

struct Light
{
   vec3 position;
   vec3 color;
} light[4];

void main()
{
    float dt = fs_in.dt * 0.1f;
    light[0].position = vec3(-10.0f, 10.0f, 10.0f);
	light[0].color = vec3(300.0f);

    light[1].position = vec3(10.0f, 10.0f, 10.0f);
	light[1].color = vec3(300.0f);

    light[2].position = vec3(-10.0f, -10.0f, 10.0f);
	light[2].color = vec3(300.0f);

    light[3].position = vec3(10.0f, -10.0f, 10.0f);
	light[3].color = vec3(300.0f);

	Material material = aMaterialData[fs_in.matId];
    vec3 n = normalize(fs_in.normal);
    vec3 v = normalize(fs_in.viewDir);

	float NoV =	abs(dot(n, v)) + 1e-5;
	vec3 Lo = vec3(0.0f);

	vec3 f0	= mix(vec3(0.04), material.albedo.rgb, material.metallic);
	for(int i = 0; i < 4; ++i)
	{
    	vec3 l = light[i].position - fs_in.worldPos;
		float dist = length(l);
		l /= dist;
    	vec3 h = normalize(v + l);

		float NoL =	clamp(dot(n, l), 0.0, 1.0);
		float LoH =	clamp(dot(l, h), 0.0, 1.0);
		float NoH =	clamp(dot(n, h), 0.0, 1.0);

		float alpha	= material.roughness * material.roughness;

		float D	= D_GGX(NoH, alpha);
		vec3  F	= F_Schlick(LoH, f0);
		float V	= V_SmithGGXCorrelatedFast(NoV,	NoL, alpha);

		vec3  Ks = F;
		vec3  Kd = (1.0f - Ks) * (1.0f - material.metallic);
		vec3 specular =	D *	F *	V;
		float attenuation = 1.0f;

		#if 1
    		attenuation = 1.0f / (dist * dist);
		#endif
		Lo	+= (Kd * (material.albedo.rgb / PI) + specular) * NoL * light[i].color * attenuation;
	}

	vec3 Ks = F_Schlick(NoV, f0);
	vec3 Kd = (1.0f - Ks) * (1.0f - material.metallic);
	vec3 irradiance = texture(uIrradianceMap, n).rgb;
	Lo += (irradiance * material.albedo.rgb) * Kd * material.ao;

	Lo /=(1.0f + Lo);
    Lo = pow(Lo, vec3(0.4545));
	fragColor =	vec4(Lo, 1.0f);
}