#pragma once

#include "../Engine/GlmIncludes.h"
#include "MathUtils.h"

#include <vector>
#include <string>

constexpr const uint32_t kMaxLODs    = 8;
constexpr const uint32_t kMaxStreams = 8;
constexpr const uint32_t EMPTY_NODE = 0xffffffff;
inline uint8_t PackFloatToU8(float val)
{
	return uint8_t(val * 127.0f + 127.5f);
}

struct Vertex
{
	float px, py, pz;
	uint8_t nx, ny, nz;
	// uv coordinate
	float ux, uy;
	// tangent and bitangent
	uint8_t tx, ty, tz;
	uint8_t bx, by, bz;

	Vertex() = default;
	Vertex(glm::vec3 position, glm::vec3 normal, glm::vec2 uv, glm::vec3 tangent = glm::vec3(0.0f), glm::vec3 bitangent = glm::vec3(0.0f)) :
		px(position.x), py(position.y), pz(position.z),
		nx(PackFloatToU8(normal.x)), ny(PackFloatToU8(normal.y)), nz(PackFloatToU8(normal.z)),
		ux(uv.x), uy(uv.y),
		tx(PackFloatToU8(tangent.x)), ty(PackFloatToU8(tangent.y)), tz(PackFloatToU8(tangent.z)),
		bx(PackFloatToU8(bitangent.x)), by(PackFloatToU8(bitangent.y)), bz(PackFloatToU8(bitangent.z))
	{
	}
};

#define NUM_BONE_PER_VERTEX 4
struct AnimatedVertex : public Vertex
{
	AnimatedVertex() = default;
	AnimatedVertex(glm::vec3 position, glm::vec3 normal, glm::vec2 uv, glm::vec3 tangent = glm::vec3(0.0f), glm::vec3 bitangent = glm::vec3(0.0f)) :
		Vertex(position, normal, uv, tangent, bitangent)
	{
		for (uint32_t i = 0; i < NUM_BONE_PER_VERTEX; ++i)
		{
			ids[i] = 255;
			weights[i] = 0.0f;
		}
	}

	void setVertexWeight(uint32_t boneId, float weight)
	{
		for (uint32_t i = 0; i < NUM_BONE_PER_VERTEX; ++i)
		{
			if (ids[i] == 255)
			{
				ids[i] = boneId;
				weights[i] = weight;
				return;
			}
		}
		fprintf(stderr, "Vertex has influence of more than 4 bones");
	}

	// BoneId
	uint8_t ids[NUM_BONE_PER_VERTEX];

	// Bone Weight/Influence
	float weights[NUM_BONE_PER_VERTEX];
};

struct Node {
	char name[64];
	int parent;
	int firstChild;
	int nextSibling;
	// Not like last but more like last added sibling
	int lastSibling;
	glm::mat4 localTransform;
};

struct SkeletonNode : public Node {
	int boneIndex;
	glm::mat4 offsetMatrix;
};

struct Mesh final
{
	uint32_t vertexOffset = 0;

	uint32_t indexOffset = 0;

	uint32_t vertexCount = 0;

	uint32_t indexCount = 0;

	uint32_t materialIndex = 0;

	char meshName[32];

	uint32_t nodeIndex;

	uint32_t skeletonIndex;

	uint32_t boneCount;
};

struct MeshFileHeader
{
	// Check integrity
	uint32_t magicNumber;

	uint32_t nodeCount;

	uint32_t skeletonNodeCount;

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
	std::vector<Node> nodes_;
	std::vector<SkeletonNode> skeletonNodes_;
	std::vector<Mesh> meshes_;
	std::vector<MaterialComponent> materials_;
	std::vector<std::string> textures_;
	std::vector<BoundingBox> boxes_;
	std::vector<uint8_t> vertexData_;
	std::vector<uint32_t> indexData_;
};

static_assert(sizeof(MaterialComponent) % 16 == 0);
