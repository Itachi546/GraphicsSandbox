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

	ecs::Entity CreateCube(std::string_view name);
	ecs::Entity CreatePlane(std::string_view name);
	ecs::Entity CreateSphere(std::string_view name);
	ecs::Entity CreateMesh(const char* file);
	ecs::Entity CreateLight(std::string_view name = "");

	void Update(float dt);

	void GenerateDrawData(std::vector<DrawData>& out);

	void SetSize(int width, int height);

	void Destroy(ecs::Entity& entity);

	Camera* GetCamera() {
		return &mCamera;
	}

	inline std::unique_ptr<EnvironmentMap>& GetEnvironmentMap() { return mEnvMap; }

	std::vector<ecs::Entity> FindChildren(ecs::Entity entity);

	~Scene(); 


private:
	uint32_t mCubeMeshId = 0;
	uint32_t mSphereMeshId = 0;
	uint32_t mPlaneMeshId = 0;

	ecs::Entity mPrimitives;
	ecs::Entity mSun;

	Camera mCamera;

	std::shared_ptr<ecs::ComponentManager> mComponentManager;
	std::unique_ptr<EnvironmentMap> mEnvMap;

	void UpdateTransform();
	void UpdateHierarchy();

	void updateChildren(ecs::Entity entity, const glm::mat4& parentTransform);

	void InitializePrimitiveMesh();
	void InitializeCubeMesh(MeshDataComponent* meshComp, unsigned int& vertexCount, unsigned int& indexCount);
	void InitializePlaneMesh(MeshDataComponent* meshComp, unsigned int& vertexCount, unsigned int& indexCount, uint32_t subdivide = 2);
	void InitializeSphereMesh(MeshDataComponent* meshComp, unsigned int& vertexCount, unsigned int& indexCount);

	void InitializeLights();

	friend class Renderer;
};

/*
* Create a Buffer Of Arbitary Size
* Push Data
* Push Data
*/
