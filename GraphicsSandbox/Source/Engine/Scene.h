#pragma once

#include "ECS.h"
#include "Components.h"

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
	Scene();

	Scene(const Scene&) = delete;
	void operator=(const Scene&) = delete;

	std::shared_ptr<ecs::ComponentManager> GetComponentManager()
	{
		return mComponentManager;
	}

	ecs::Entity CreateMesh(std::string_view name);
	ecs::Entity CreateCube(std::string_view name);

	void Update(float dt);

	void GenerateDrawData(std::vector<DrawData>& out);

	void Destroy(const ecs::Entity& entity);

	~Scene() {
		ecs::DestroyEntity(mComponentManager.get(), mCubeEntity);
	}

private:
	ecs::Entity mCubeEntity;
	std::shared_ptr<ecs::ComponentManager> mComponentManager;

	void UpdateTransformData();
};
