#version 450

#extension GL_GOOGLE_include_directive: require

layout(location = 0) out vec4 fragColor;

layout(location = 0) in VS_OUT 
{
   vec3 normal;
   vec3 worldPos;
   vec3 viewDir;
   float dt;
} fs_in;

#define FRAGMENT_SHADER
#include "pbr.glsl"
#include "bindings.glsl"

const vec3 viewPos = vec3(0.0, 3.0, 5.0);

struct Light
{
   vec3 position;
   vec3 color;
   float intensity;
} light;

void main()
{
    float dt = fs_in.dt * 0.1f;
    light.position = vec3(3.0f * cos(dt), 2.0, 4.0f * sin(dt));
	light.color = vec3(1.0f);
	light.intensity = 4.0f;

    vec3 albedo = vec3(1.0f, 1.0f, 1.0f);
    float roughness = 0.9f;
    float metallic = 0.2f;

    vec3 n = normalize(fs_in.normal);
    vec3 v = normalize(fs_in.viewDir);

	vec3 Lo = vec3(0.0f);
	vec3 l = normalize(light.position - fs_in.worldPos);
	vec3 h = normalize(v + l);

	float NoV =	abs(dot(n, v)) + 1e-5;
	float NoL =	clamp(dot(n, l), 0.0, 1.0);
	float NoH =	clamp(dot(n, h), 0.0, 1.0);
	float LoH =	clamp(dot(l, h), 0.0, 1.0);
	float alpha	= roughness	* roughness;

	vec3 f0	= mix(vec3(0.04), albedo, metallic);
	float D	= D_GGX(NoH, alpha);
	vec3  F	= F_Schlick(LoH, f0);
	float V	= V_SmithGGXCorrelatedFast(NoV,	NoL, alpha);
	
	vec3  Ks = F;
	vec3  Kd = (1.0f - Ks) * (1.0f - metallic);

	vec3 specular =	D *	F *	V;
	Lo	+= (Kd * albedo	/ PI + specular) * NoL * light.color * light.intensity;

	Lo /=(1.0f + Lo);
    Lo = pow(Lo, vec3(0.4545));
	fragColor =	vec4(Lo, 1.0f);
}