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

constexpr int NUM_BONE_PER_VERTEX = 4;
struct AnimatedVertex : public Vertex
{
	AnimatedVertex() = default;
	AnimatedVertex(glm::vec3 position, glm::vec3 normal, glm::vec2 uv, glm::vec3 tangent = glm::vec3(0.0f), glm::vec3 bitangent = glm::vec3(0.0f)) :
		Vertex(position, normal, uv, tangent, bitangent)
	{
		for (uint32_t i = 0; i < NUM_BONE_PER_VERTEX; ++i)
		{
			ids[i] = -1;
			weights[i] = 0.0f;
		}
	}

	void setVertexWeight(uint32_t boneId, float weight)
	{
		for (uint32_t i = 0; i < NUM_BONE_PER_VERTEX; ++i)
		{
			if (ids[i] == -1)
			{
				ids[i] = boneId;
				weights[i] = weight;
				return;
			}
		}
		fprintf(stderr, "Vertex has influence of more than 4 bones");
	}

	// BoneId
	int ids[NUM_BONE_PER_VERTEX];

	// Bone Weight/Influence
	float weights[NUM_BONE_PER_VERTEX];
};

constexpr uint32_t MAX_MESHLET_TRIANGLES = 124;
constexpr uint32_t MAX_MESHLET_VERTICES = 64;

struct Meshlet {

	// Offset specified in element count
	uint32_t vertexOffset;
	uint32_t vertexCount;

	// Offset specified in element count
	uint32_t triangleOffset;
	uint32_t triangleCount;

	// Cone information
	glm::vec4 cone;
};

static_assert(sizeof(Meshlet) % 16 == 0);

struct MeshDrawData {
	glm::vec4 boudingSphere;

	uint32_t vertexOffset;
	uint32_t vertexCount;
	uint32_t indexOffset;
	uint32_t indexCount;

	uint32_t meshletOffset;
	uint32_t meshletCount;
	uint32_t meshletTriangleOffset;
	uint32_t meshletTriangleCount;
};

static_assert(sizeof(MeshDrawData) % 16 == 0);

void InitializeCubeMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
void InitializePlaneMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t subdivide = 2);
void InitializeSphereMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);