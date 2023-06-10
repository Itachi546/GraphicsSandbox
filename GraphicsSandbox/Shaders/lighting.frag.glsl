#version 450
#extension GL_GOOGLE_include_directive: require

#include "pbr.glsl"
#include "globaldata.glsl"
#include "material.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) readonly buffer LightBuffer {
   LightData lightData[];
} lightBuffer;

#include "bindless.glsl"

layout(push_constant) uniform PushConstants
{
	uint uIrradianceMap;
	uint uPrefilterEnvMap;
	uint uBRDFLUT;
	uint uPositionBuffer;

	uint uNormalBuffer;
	uint uPBRBuffer;
	uint uColorBuffer;
	uint uEmissiveBuffer;

	vec3 uCameraPosition;
	float exposure;
	float globalAO;
	uint nLight;
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

vec3 CalculateColor(vec2 uv)
{
	vec4 albedo = texture(uTextures[nonuniformEXT(uColorBuffer)], uv);

	vec3 pbrFactor = texture(uTextures[nonuniformEXT(uPBRBuffer)], uv).rgb;
	float metallic = pbrFactor.r;
	float roughness = pbrFactor.g;
	float ao = pbrFactor.b * globalAO;

    vec3 n = texture(uTextures[nonuniformEXT(uNormalBuffer)], uv).rgb;

	vec3 worldPos = texture(uTextures[nonuniformEXT(uPositionBuffer)], uv).rgb;
	
    vec3 v = normalize(uCameraPosition - worldPos);
	vec3 r = reflect(-v, n);

	float NoV =	max(dot(n, v), 0.0001);
	vec3 Lo = vec3(0.0f);

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
			//shadow = CalculateShadowFactor(fs_in.worldPos, abs(fs_in.lsPos.z), cascadeIndex);
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
	vec3 emissive = texture(uTextures[nonuniformEXT(uEmissiveBuffer)], uv).rgb;

	//if(material.emissiveMap != INVALID_TEXTURE)
	//emissive = texture(uTextures[nonuniformEXT(material.emissiveMap)], fs_in.uv).rgb * material.emissive;

	Lo += ambient + emissive;

	//if(globals.enableCascadeDebug >	0)
    //   Lo *= cascadeColor[cascadeIndex] * 0.5f;

	return Lo;
}

void main()
{
	vec3 Lo = CalculateColor(uv);
 	Lo = ACESFilm(Lo * exposure);
    Lo = pow(Lo, vec3(0.4545));
	//Lo = pow(Lo, vec3(0.4545));
	fragColor =	vec4(Lo, 1.0f);
}