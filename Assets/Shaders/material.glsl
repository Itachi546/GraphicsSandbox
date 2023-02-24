#ifndef MATERIAL_H
#define MATERIAL_H
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
#endif