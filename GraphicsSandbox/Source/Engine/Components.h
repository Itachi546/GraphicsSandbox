#pragma once

#include "GlmIncludes.h"
#include "Graphics.h"
#include <string>
#include <memory>
#include <vector>

#include "MeshData.h"
#include "TransformComponent.h"
#include "Animation/animation.h"

#include "ECS.h"

struct PerObjectData
{
	uint32_t transformIndex;
	uint32_t materialIndex;
};

inline bool IsTextureValid(uint32_t texture)
{
	return texture != gfx::INVALID_TEXTURE_ID;
}

constexpr int ALPHAMODE_NONE = 0;
constexpr int ALPHAMODE_BLEND = 1;
constexpr int ALPHAMODE_MASK = 2;

struct MaterialComponent {
	glm::vec4 albedo = glm::vec4(1.0f);
	glm::vec3 emissive = glm::vec3(0.0f);

	float metallic = 0.01f;
	float roughness = 0.9f;
	float ao = 1.0f;
	float transparency = 1.0f;
	float alphaCutoff = 0.0f;
	int alphaMode = ALPHAMODE_NONE;

	union {
		uint32_t textures[7] = {
			gfx::INVALID_TEXTURE_ID,
			gfx::INVALID_TEXTURE_ID,
			gfx::INVALID_TEXTURE_ID,
			gfx::INVALID_TEXTURE_ID,
			gfx::INVALID_TEXTURE_ID,
			gfx::INVALID_TEXTURE_ID,
			gfx::INVALID_TEXTURE_ID
		};
		struct {
			uint32_t albedoMap;
			uint32_t normalMap;
			uint32_t emissiveMap;
			uint32_t metallicMap;
			uint32_t roughnessMap;
			uint32_t ambientOcclusionMap;
			uint32_t opacityMap;
		};
	};

	uint32_t GetTextureCount()
	{
		uint32_t count = 0;
		if (albedoMap != gfx::INVALID_TEXTURE_ID)
			count += 1;
		if (normalMap != gfx::INVALID_TEXTURE_ID)
			count += 1;
		if (emissiveMap != gfx::INVALID_TEXTURE_ID)
			count += 1;
		if (metallicMap != gfx::INVALID_TEXTURE_ID)
			count += 1;
		if (roughnessMap != gfx::INVALID_TEXTURE_ID)
			count += 1;
		if (ambientOcclusionMap != gfx::INVALID_TEXTURE_ID)
			count += 1;
		if (opacityMap != gfx::INVALID_TEXTURE_ID)
			count += 1;
		return count;
	}

	bool IsTransparent()
	{
		return alphaMode != ALPHAMODE_NONE;
	}
};
static_assert(sizeof(MaterialComponent) % 16 == 0);

struct DrawData
{
	glm::mat4 worldTransform;
	MaterialComponent* material;
	uint32_t indexCount;
	uint32_t meshletCount;
	uint32_t meshletOffset;
	gfx::BufferView vertexBuffer;
	gfx::BufferView indexBuffer;
	gfx::BufferHandle meshletVertexBuffer;
	gfx::BufferHandle meshletTriangleBuffer;
	gfx::BufferHandle meshletBuffer;

	glm::vec4 boundingSphere;

	uint32_t elmSize;
	ecs::Entity entity;
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

struct IMeshRenderer
{
	enum Flags {
		Empty = 0,
		Renderable = 1 << 0,
		DoubleSided = 1 << 1,
		Skinned = 1 << 2
	};

	uint32_t flags = Renderable;
	gfx::BufferView vertexBuffer;
	gfx::BufferView indexBuffer;

	gfx::BufferHandle meshletBuffer;
	gfx::BufferHandle meshletVertexBuffer;
	gfx::BufferHandle meshletTriangleBuffer;

	uint32_t meshletOffset;
	uint32_t meshletCount;

	BoundingBox boundingBox;
	glm::vec4 boundingSphere;

	void SetRenderable(bool value) {
		if (value) flags |= Renderable; else flags &= ~Renderable;
	}

	void SetSkinned(bool value) { 
		if (value) flags |= Skinned;
		else flags &= ~Skinned;

	}

	void SetDoubleSided(bool value) {
		if (value) flags |= DoubleSided; else flags &= ~DoubleSided;
	}

	bool IsRenderable() const { return flags & Renderable; }
	bool IsDoubleSided() const { return flags & DoubleSided; }
	bool IsSkinned() const { return flags & Skinned; }

	virtual uint32_t GetIndexCount() const = 0;

	virtual ~IMeshRenderer() = default;
};

struct MeshRenderer : public IMeshRenderer
{
	MeshRenderer() {
	}
	uint32_t GetVertexOffset() const {
		return sizeof(Vertex);
	}

	uint32_t GetIndexCount() const override {
		return indexBuffer.byteLength / sizeof(uint32_t);
	}

	virtual ~MeshRenderer() {
	}
};

struct SkinnedMeshRenderer : public IMeshRenderer
{
	SkinnedMeshRenderer() {
		SetSkinned(true);
	}

	Skeleton skeleton;

	// Animation Clip
	std::vector<AnimationClip> animationClips;

	uint32_t GetVertexOffset() const {
		assert(0);
	}

	void AddAnimationClip(AnimationClip animationClip)
	{
		animationClips.push_back(std::move(animationClip));
	}
	
	uint32_t GetIndexCount() const override {
		return static_cast<uint32_t>(indexBuffer.byteLength / sizeof(uint32_t));
	}

	virtual ~SkinnedMeshRenderer() = default;
};
