#pragma once

#include "ECS.h"
#include "Components.h"
#include "GpuMemoryAllocator.h"
#include "EnvironmentMap.h"
#include "Camera.h"

#include <string_view>

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

	void GenerateDrawData(std::vector<DrawData>& out);

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

	~Scene(); 


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

	void UpdateTransform();
	void UpdateHierarchy();

	void UpdateChildren(ecs::Entity entity, const glm::mat4& parentTransform);

	struct StagingMeshData
	{
		std::vector<Node> nodes;
		std::vector<SkeletonNode> skeletonNodes;
		std::vector<Mesh> meshes;
		std::vector<BoundingBox> boundingBoxes;
		std::vector<char> vertices;
		std::vector<char> indices;
		std::vector<MaterialComponent> materials;
		std::vector<std::string> textures;
		std::shared_ptr<gfx::GPUBuffer> vertexBuffer;
		std::shared_ptr<gfx::GPUBuffer> indexBuffer;
	};

	ecs::Entity CreateMeshEntity(uint32_t nodeIndex,
		ecs::Entity parent,
		const StagingMeshData& stagingData);

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

	friend class Renderer;
};

/*
* Create a Buffer Of Arbitary Size
* Push Data
* Push Data
*/
