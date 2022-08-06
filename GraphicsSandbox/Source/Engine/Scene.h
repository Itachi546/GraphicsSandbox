#pragma once

#include "ECS.h"
#include "Components.h"
#include "GpuMemoryAllocator.h"

#include <string_view>

class Scene
{
public:
	/*
	* HACK:
	* Inlining and Static variable
	* When the components are registered from here 
	* the componentId returned from the static GetID() function
	* is incorrect.
	* Declaring it in the .cpp file fix the issues
	*/
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

	void Destroy(const ecs::Entity& entity);

	~Scene(); 


private:
	ecs::Entity mCubeEntity;
	ecs::Entity mPlaneEntity;
	ecs::Entity mSphereEntity;

	std::shared_ptr<ecs::ComponentManager> mComponentManager;

	void UpdateTransformData();

	void InitializePrimitiveMesh();
	void InitializeCubeMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount);
	void InitializePlaneMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount);
	void InitializeSphereMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount);
};

/*
* Create a Buffer Of Arbitary Size
* Push Data
* Push Data
*/
