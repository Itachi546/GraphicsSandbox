#pragma once

#include "ECS.h"
#include "Components.h"
#include "EnvironmentMap.h"
#include "Camera.h"

#include <string_view>
#include <vector>

class Scene
{
public:

	Scene(){}

	void Initialize();

	Scene(const Scene&) = delete;
	void operator=(const Scene&) = delete;

	std::shared_ptr<ecs::ComponentManager> GetComponentManager()
	{
		return mComponentManager;
	}

	ecs::Entity GetSun() { return mSun; }

	ecs::Entity CreateCube(std::string_view name);
	ecs::Entity CreatePlane(std::string_view name);
	ecs::Entity CreateSphere(std::string_view name);
	ecs::Entity CreateMesh(const char* file);
	ecs::Entity CreateLight(std::string_view name = "");

	void Update(float dt);

	void SetSize(int width, int height);

	void SetShowBoundingBox(bool state) { mShowBoundingBox = state; }
	bool GetShowBoundingBox() { return mShowBoundingBox; }

	void SetEnableFrustumCulling(bool state) { mEnableFrustumCulling = state; }

	void Destroy(ecs::Entity entity);

	Camera* GetCamera() {
		return &mCamera;
	}

	inline std::unique_ptr<EnvironmentMap>& GetEnvironmentMap() { return mEnvMap; }

	std::vector<ecs::Entity> FindChildren(ecs::Entity entity);

	std::vector<DrawData>& GetStaticDrawData() {
		return mDrawData;
	}

	std::vector<DrawData>& GetSkinnedMeshDrawData()
	{
		return mSkinnedMeshDrawData;
	}

	void Shutdown();
	virtual ~Scene() = default; 


private:
	// Default Entities
	ecs::Entity mCube;
	ecs::Entity mSphere;
	ecs::Entity mPlane;

	ecs::Entity mSun;

	Camera mCamera;
	bool mShowBoundingBox = false;
	bool mEnableFrustumCulling = true;

	std::shared_ptr<ecs::ComponentManager> mComponentManager;
	std::unique_ptr<EnvironmentMap> mEnvMap;
	std::vector<gfx::BufferHandle> mAllocatedBuffers;

	std::vector<DrawData> mDrawData;
	std::vector<DrawData> mSkinnedMeshDrawData;

	void GenerateDrawData(std::vector<DrawData>& out);
	void GenerateSkinnedMeshDrawData(std::vector<DrawData>& out);

	void UpdateTransform();
	void UpdateHierarchy();

	void UpdateChildren(ecs::Entity entity, const glm::mat4& parentTransform);

	struct StagingMeshData : public MeshData
	{
		gfx::BufferHandle vertexBuffer;
		gfx::BufferHandle indexBuffer;
	};

	ecs::Entity CreateMeshEntity(uint32_t nodeIndex,
		ecs::Entity parent,
		const StagingMeshData& stagingData);

	void ParseSkeleton(const Mesh& mesh, Skeleton& skeleton, uint32_t rootBone, const std::vector<SkeletonNode>& skeletonNodes);
	void ParseAnimation(const StagingMeshData& mesh, std::vector<AnimationClip>& animationClips);
	void UpdateEntity(
		ecs::Entity entity,
		uint32_t nodeIndex,
		const StagingMeshData& stagingMeshData
	);

	ecs::Entity TraverseNode(
		uint32_t root,
		ecs::Entity parent,
		const StagingMeshData& stagingMeshData);

	void InitializePrimitiveMeshes();
	void InitializeCubeMesh(MeshRenderer& meshRenderer);
	void InitializePlaneMesh(MeshRenderer& meshRenderer, uint32_t subdivide = 2);
	void InitializeSphereMesh(MeshRenderer& meshRenderer);

	void DrawBoundingBox();
	void InitializeLights();

	void RemoveChild(ecs::Entity parent, ecs::Entity child);

	void GenerateMeshData(ecs::Entity entity, const IMeshRenderer* meshRenderer, std::vector<DrawData>& out);

	friend class Renderer;
};

/*
* Create a Buffer Of Arbitary Size
* Push Data
* Push Data
*/
