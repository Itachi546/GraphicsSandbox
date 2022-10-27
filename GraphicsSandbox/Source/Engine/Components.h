#pragma once

#include "GlmIncludes.h"
#include "Graphics.h"
#include <string>
#include <memory>
#include <vector>

#include "../Shared/MeshData.h"
#include "GpuMemoryAllocator.h"
#include "ECS.h"

struct PerObjectData
{
	uint32_t transformIndex;
	uint32_t materialIndex;
};

struct MaterialComponent;
struct DrawData
{
	glm::mat4 worldTransform;
	MaterialComponent* material;
	uint32_t indexCount;

	gfx::BufferView vertexBuffer;
	gfx::BufferView indexBuffer;
};

struct TransformComponent
{
	glm::vec3 position{ 0.0f, 0.0f, 0.0f };
	glm::vec3 rotation{ 0.0f, 0.0f, 0.0f};
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

	glm::mat4 defaultMatrix{ 1.0f };
	glm::mat4 localMatrix{1.0f};
	glm::mat4 worldMatrix{ 1.0f };

	bool dirty = true;
	void CalculateWorldMatrix()
	{
		if (dirty)
		{
			localMatrix = glm::translate(glm::mat4(1.0f), position) *
				          glm::yawPitchRoll(rotation.y, rotation.x, rotation.z) *
				          glm::scale(glm::mat4(1.0f), scale) * defaultMatrix;
			worldMatrix = localMatrix;
			dirty = false;
		}
	}

	glm::mat4 GetRotationMatrix()
	{
		return glm::yawPitchRoll(rotation.y, rotation.x, rotation.z);
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

	AABBComponent(glm::vec3* vertices, uint32_t count) : min(std::numeric_limits<float>::max()), 
		max(std::numeric_limits<float>::min())
	{
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

	uint32_t ibIndex = 0;
	gfx::GPUBuffer* vertexBuffer;

	uint32_t vbIndex = 0;
	gfx::GPUBuffer* indexBuffer;

	std::vector<Mesh> meshes;
	std::vector<BoundingBox> boundingBoxes;

	void SetRenderable(bool value) {
		if (value) flags |= Renderable; else flags &= ~Renderable;
	}

	void SetDoubleSided(bool value) {
		if (value) flags |= DoubleSided; else flags &= ~DoubleSided;
	}

	bool IsRenderable() const { return flags & Renderable; }
	bool IsDoubleSided() const { return flags & DoubleSided; }

	void CopyDataToBuffer(gfx::GpuMemoryAllocator* allocator, gfx::GPUBuffer* vB, gfx::GPUBuffer* iB, uint32_t vbIndex = 0, uint32_t ibIndex = 0);

	const Mesh* GetMesh(uint32_t meshId) const
	{
		return &meshes[meshId];
	}

	~MeshDataComponent() = default;
};

struct ObjectComponent
{
	std::size_t meshComponentIndex;
	uint32_t meshId;
};

enum class LightType
{
	Directional = 0,
	Point
};

struct LightComponent
{
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	float radius = 1;
	LightType type = LightType::Point;
};

struct HierarchyComponent
{
	ecs::Entity parent = ecs::INVALID_ENTITY;
	std::vector<ecs::Entity> childrens;

	bool RemoveChild(ecs::Entity child)
	{
		auto found = std::find(childrens.begin(), childrens.end(), child);
		if (found != childrens.end())
		{
			childrens.erase(found);
			return true;
		}
		return false;
	}
};