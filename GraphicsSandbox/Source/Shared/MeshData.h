#pragma once

#include "../Engine/GlmIncludes.h"
#include "MathUtils.h"

#include <vector>
#include <string>

constexpr const uint32_t kMaxLODs    = 8;
constexpr const uint32_t kMaxStreams = 8;

struct Mesh final
{
	uint32_t vertexOffset = 0;

	uint32_t indexOffset = 0;

	uint32_t vertexCount = 0;

	uint32_t indexCount = 0;

	uint32_t materialIndex = 0;
	char meshName[32];
};

struct MeshFileHeader
{
	// Check integrity
	uint32_t magicNumber;

	uint32_t meshCount;

	uint32_t materialCount;

	uint32_t textureCount;

	uint32_t dataBlockStartOffset;

	uint32_t vertexDataSize;
	
	uint32_t indexDataSize;

};

constexpr const uint32_t INVALID_TEXTURE = 0xFFFFFFFF;

inline bool IsTextureValid(uint32_t texture)
{
	return texture != INVALID_TEXTURE;
}
struct MaterialComponent {
	glm::vec4 albedo = glm::vec4(1.0f);
	
	float emissive = 0.0f;
	float roughness = 0.9f;
	float metallic = 0.01f;
	float ao = 1.0f;

	float transparency = 1.0f;

	union {
		uint32_t textures[7] = {
			INVALID_TEXTURE,
			INVALID_TEXTURE,
			INVALID_TEXTURE,
			INVALID_TEXTURE,
			INVALID_TEXTURE,
			INVALID_TEXTURE,
			INVALID_TEXTURE
		};
		struct {
			uint32_t albedoMap;
			uint32_t normalMap;
			uint32_t emissiveMap;
			uint32_t metallicMap;
			uint32_t roughnessMap;
			uint32_t ambientOcclusionMap;
			uint32_t opacityMap;;
		};
	};

	uint32_t GetTextureCount()
	{
		uint32_t count = 0;
		if (albedoMap != INVALID_TEXTURE)
			count += 1;
		if (normalMap != INVALID_TEXTURE)
			count += 1;
		if (emissiveMap != INVALID_TEXTURE)
			count += 1;
		if (metallicMap != INVALID_TEXTURE)
			count += 1;
		if (roughnessMap != INVALID_TEXTURE)
			count += 1;
		if (ambientOcclusionMap != INVALID_TEXTURE)
			count += 1;
		if (opacityMap != INVALID_TEXTURE)
			count += 1;
		return count;
	}
};

struct MeshData
{
	std::vector<float> vertexData_;
	std::vector<uint32_t> indexData_;
	std::vector<Mesh> meshes;
	std::vector<MaterialComponent> materials;
	std::vector<std::string> textures;
	std::vector<BoundingBox> boxes_;
};


static_assert(sizeof(MaterialComponent) % 16 == 0);