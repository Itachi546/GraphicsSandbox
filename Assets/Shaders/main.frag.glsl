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
	vec4 albedo = material.albedo;
	float roughness = material.roughness;
	float ao = material.ao;
	float metallic = material.metallic;


    vec3 n = normalize(fs_in.normal);
    vec3 v = normalize(fs_in.viewDir);
	vec3 r = reflect(-v, n);

	float NoV =	abs(dot(n, v));
	vec3 Lo = vec3(0.0f);

	vec3 f0	= mix(vec3(0.04), albedo.rgb, metallic);
	for(int i = 0; i < 4; ++i)
	{
    	vec3 l = light[i].position - fs_in.worldPos;
		float dist = length(l);
		l /= dist;
    	vec3 h = normalize(v + l);

		float NoL =	clamp(dot(n, l), 0.0, 1.0);
		float LoH =	clamp(dot(l, h), 0.0, 1.0);
		float NoH =	clamp(dot(n, h), 0.0, 1.0);

		float alpha	= roughness * roughness;

		float D	= D_GGX(NoH, alpha);
		vec3  F	= F_Schlick(LoH, f0);
		float V	= V_SmithGGXCorrelated(NoV,	NoL, roughness);

		vec3  Ks = F;
		vec3  Kd = (1.0f - Ks) * (1.0f - metallic);
		vec3 specular =	D *	F *	V / (4.0f * NoV * NoL + 0.0001f);

		float attenuation = 1.0f;
		#if 1
    		attenuation = 1.0f / (dist * dist);
		#endif
		Lo	+= (Kd * (albedo.rgb / PI) + specular) * NoL * light[i].color * attenuation;
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
	Lo += ambient;

	Lo /=(1.0f + Lo);
    Lo = pow(Lo, vec3(0.4545));
	fragColor =	vec4(Lo, 1.0f);
}