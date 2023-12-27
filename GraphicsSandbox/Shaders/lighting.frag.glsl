 #version 450
#extension GL_GOOGLE_include_directive: require

#include "pbr.glsl"
#include "globaldata.glsl"
#include "material.glsl"
#include "utils.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

layout(binding = 0) readonly buffer LightBuffer {
   LightData lightData[];
} lightBuffer;

layout(binding = 1, std140) uniform CascadeInfo
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
	uint uIrradianceMap;
	uint uPrefilterEnvMap;
	uint uBRDFLUT;
	uint uPositionBuffer;

	uint uNormalBuffer;
	uint uPBRBuffer;
	uint uColorBuffer;
	uint uEmissiveBuffer;
	
	uint uSSAOBuffer;
	uint directionalShadowMap;
	float exposure;
	float globalAO;

	vec3 uCameraPosition;
	uint nLight;

	uint enableShadow;
	uint enableAO;
	float bloomThreshold;
};


float AttenuationSquareFallOff(float lightDistSqr, float invRadius) {
   const float factor = lightDistSqr * invRadius * invRadius;
   const float smoothFactor = max(1.0f - factor * factor, 0.0f);
   return smoothFactor * smoothFactor / max(lightDistSqr, 1e-4);
}

vec3 CalculateColor(vec2 uv)
{
	vec3 emissive = texture(uTextures[nonuniformEXT(uEmissiveBuffer)], uv).rgb;
	if(dot(emissive, vec3(1.0f)) > 0.001f) return emissive * 5.0f;

	vec4 albedo = texture(uTextures[nonuniformEXT(uColorBuffer)], uv);

	vec3 pbrFactor = texture(uTextures[nonuniformEXT(uPBRBuffer)], uv).rgb;
	float metallic = pbrFactor.r;
	float roughness = pbrFactor.g;
	//float ao = pbrFactor.b * globalAO;
	float occlusion = 1.0f;
	if(enableAO > 0.5)
    	occlusion = texture(uTextures[nonuniformEXT(uSSAOBuffer)],	uv).r;

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
		if(light.type > 0.5f)
		{
            vec3 lightDir = light.position - worldPos;
		    float dist = length(lightDir);
			attenuation	= AttenuationSquareFallOff(dist * dist, 1.0f / light.radius);
			l = lightDir / dist;
        }
		else if(light.type < 0.5f)
		{
    		l= normalize(l);
			float camDist = length(uCameraPosition - worldPos);
			shadow = enableShadow > 0 ? CalculateShadowFactor(worldPos, camDist, directionalShadowMap, cascadeIndex) : 1.0;
		}

    	vec3 h = normalize(v + l);
		float NoL =	clamp(dot(n, l), 0.0, 1.0);
		float LoH =	clamp(dot(l, h), 0.0, 1.0);
		float NoH =	clamp(dot(n, h), 0.0, 1.0);

		float D	= D_GGX(NoH, roughness * roughness);
		vec3  F	= F_Schlick(LoH, f0);
		float V	= V_SmithGGX(NoV, NoL, roughness);

		vec3  Ks = F;
		vec3  Kd = vec3(1.0f - Ks) * (1.0f - metallic);
		vec3 specular =	(V * F * D) / (4.0f * NoV * NoL + 0.0001f);

		Lo	+= (Kd * (albedo.rgb / PI) + specular) * NoL * light.color * attenuation * shadow * occlusion;
	}

	vec3 Ks = F_SchlickRoughness(NoV, f0, roughness);
	vec3 Kd = (1.0f - Ks) * (1.0f - metallic);
	vec3 irradiance = texture(uTexturesCube[nonuniformEXT(uIrradianceMap)], n).rgb;
	vec3 diffuse = irradiance * albedo.rgb;

	const float MAX_REFLECTION_LOD = 6.0;
	vec3 prefilteredColor = textureLod(uTexturesCube[nonuniformEXT(uPrefilterEnvMap)], r, roughness * MAX_REFLECTION_LOD).rgb;
	vec2 brdf = texture(uTextures[nonuniformEXT(uBRDFLUT)], vec2(NoV, roughness)).rg;
	vec3 specular = prefilteredColor * (Ks * brdf.x + brdf.y);

	vec3 ambient = (Kd * diffuse + specular) * globalAO;


	Lo += ambient + emissive;

	#if ENABLE_CASCADE_DEBUG
     Lo *= cascadeColor[cascadeIndex] * 0.5;
	#endif

	return Lo;
}

void main()
{
	vec3 Lo = CalculateColor(uv);
	fragColor =	vec4(Lo, 1.0f);
	if(rgb2Luma(Lo) > bloomThreshold) 
	  brightColor = fragColor;
    else
	 brightColor = vec4(0.0f);
	/*
 	Lo = ACESFilm(Lo * exposure);
    Lo = pow(Lo, vec3(0.4545));
	fragColor =	vec4(Lo, 1.0f);
	*/
}