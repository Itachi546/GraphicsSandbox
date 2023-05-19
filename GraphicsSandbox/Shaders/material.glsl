#ifndef MATERIAL_H
#define MATERIAL_H

const int ALPHAMODE_NONE = 0;
const int ALPHAMODE_BLEND = 1;
const int ALPHAMODE_MASK = 2;

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