#ifndef MATERIAL_H
#define MATERIAL_H

#define ALPHAMODE_NONE   0
#define ALPHAMODE_BLEND   1
#define ALPHAMODE_MASK 2

struct Material
{
	vec4 albedo;
	vec3 emissive;

	float metallic;
	float roughness;
	float ao;
	float transparency;
	float alphaCutoff;
	int alphaMode;

	uint albedoMap;
	uint normalMap;
	uint emissiveMap;
	uint metallicMap;
	uint roughnessMap;
	uint ambientOcclusionMap;
	uint opacityMap;
};

#endif