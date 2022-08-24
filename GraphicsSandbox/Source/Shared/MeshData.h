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
	char meshName[32];
};

struct MeshFileHeader
{
	// Check integrity
	uint32_t magicNumber;

	uint32_t meshCount;

	uint32_t dataBlockStartOffset;

	uint32_t vertexDataSize;
	
	uint32_t indexDataSize;

};

struct MeshData
{
	std::vector<float> vertexData_;
	std::vector<uint32_t> indexData_;
	std::vector<Mesh> meshes;
	std::vector<BoundingBox> boxes_;
};

