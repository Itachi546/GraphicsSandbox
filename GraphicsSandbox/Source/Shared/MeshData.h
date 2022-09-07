#pragma once

#include "../Engine/GlmIncludes.h"
#include "MathUtils.h"

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

	uint32_t dataBlockStartOffset;

	uint32_t vertexDataSize;
	
	uint32_t indexDataSize;

};

constexpr const uint32_t INVALID_TEXTURE = 0xFFFFFFFF;

struct MaterialComponent {
	glm::vec4 albedo = glm::vec4(1.0f);
	
	float emissive = 0.0f;
	float roughness = 0.1f;
	float metallic = 1.0f;
	float ao = 1.0f;

	float transparency = 1.0f;

	uint32_t albedoMap = INVALID_TEXTURE;
	uint32_t normalMap = INVALID_TEXTURE;
	uint32_t emissiveMap = INVALID_TEXTURE;
	uint32_t metallicMap = INVALID_TEXTURE;

	uint32_t roughnessMap = INVALID_TEXTURE;
	uint32_t ambientOcclusionMap = INVALID_TEXTURE;
	uint32_t opacityMap = INVALID_TEXTURE;
};

struct MeshData
{
	std::vector<float> vertexData_;
	std::vector<uint32_t> indexData_;
	std::vector<Mesh> meshes;
	std::vector<MaterialComponent> materials;
	std::vector<BoundingBox> boxes_;
};


static_assert(sizeof(MaterialComponent) % 16 == 0);