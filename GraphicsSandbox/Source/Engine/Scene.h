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

	void Update(float dt);

	void GenerateDrawData(std::vector<DrawData>& out);

	void Destroy(ecs::Entity& entity);

	Camera* GetCamera() {
		return &mCamera;
	}

	inline std::unique_ptr<EnvironmentMap>& GetEnvironmentMap() { return mEnvMap; }

	~Scene(); 


private:
	ecs::Entity mCubeEntity;
	ecs::Entity mPlaneEntity;
	ecs::Entity mSphereEntity;

	Camera mCamera;

	std::shared_ptr<ecs::ComponentManager> mComponentManager;
	std::unique_ptr<EnvironmentMap> mEnvMap;
	void UpdateTransformData();

	void InitializePrimitiveMesh();
	void InitializeCubeMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount);
	void InitializePlaneMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount);
	void InitializeSphereMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount);

	friend class Renderer;
};

/*
* Create a Buffer Of Arbitary Size
* Push Data
* Push Data
*/
