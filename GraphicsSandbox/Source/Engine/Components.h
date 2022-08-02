#pragma once

#include "GlmIncludes.h"
#include "Graphics.h"
#include <string>
#include <memory>
#include <vector>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
};

struct PerObjectData
{
	glm::mat4 transform;

	PerObjectData(){}
	PerObjectData(const glm::mat4& transform) : transform(transform) {}
};

struct DrawData
{
	gfx::GPUBuffer* vertexBuffer;
	gfx::GPUBuffer* indexBuffer;
	glm::mat4 worldTransform;
	uint32_t indexCount;
};

struct TransformComponent
{
	glm::vec3 position{ 0.0f, 0.0f, 0.0f };
	glm::fquat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

	glm::mat4 world{ 1.0f };
	bool dirty = true;

	void SetDirty(bool state)
	{
		this->dirty = state;
	}

	void CalculateWorldMatrix()
	{
		world = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
		this->dirty = false;
	}
};

struct NameComponent
{
	std::string name;
};

struct AABBComponent
{
	glm::vec3 min;
	glm::vec3 max;

	AABBComponent(glm::vec3* vertices, uint32_t count)
	{
		min = glm::vec3(std::numeric_limits<float>::max());
		max = glm::vec3(std::numeric_limits<float>::min());
		for (uint32_t i = 0; i < count; ++i)
		{
			min = glm::min(min, vertices[i]);
			max = glm::max(max, vertices[i]);
		}
	}

	AABBComponent(const glm::vec3& min, const glm::vec3& max) : min(min), max(max)
	{
	}
};

struct MeshDataComponent
{
	enum Flags {
		Empty = 0,
		Renderable = 1 << 0,
		DoubleSided = 1 << 1,
	};
	uint32_t flags = Renderable;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	gfx::GPUBuffer vertexBuffer;
	gfx::GPUBuffer indexBuffer;

	uint32_t GetNumIndices() const { return static_cast<uint32_t>(indices.size()); }

	void SetRenderable(bool value) {
		if (value) flags |= Renderable; else flags &= ~Renderable;
	}

	void SetDoubleSided(bool value) {
		if (value) flags |= DoubleSided; else flags &= ~DoubleSided;
	}

	bool IsRenderable() const { return flags & Renderable; }
	bool IsDoubleSided() const { return flags & DoubleSided; }

	void CreateRenderData();

	~MeshDataComponent() = default;
};

struct ObjectComponent
{
	std::size_t meshId;
};