#include "Scene.h"
#include "Logger.h"

Scene::Scene()
{
	mComponentManager = std::make_shared<ecs::ComponentManager>();

	mComponentManager->RegisterComponent<TransformComponent>();
	mComponentManager->RegisterComponent<MeshDataComponent>();
	mComponentManager->RegisterComponent<ObjectComponent>();
	mComponentManager->RegisterComponent<NameComponent>();
}

ecs::Entity Scene::CreateMesh(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	/*
	if(!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;
		*/
	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<MeshDataComponent>(entity);
	return entity;
}

void Scene::GenerateDrawData(std::vector<DrawData>& out)
{
	auto objectComponentArray = mComponentManager->GetComponentArray<ObjectComponent>();
	auto meshDataComponentArray = mComponentManager->GetComponentArray<MeshDataComponent>();

	for (int i = 0; i < objectComponentArray->GetCount(); ++i)
	{
		DrawData drawData = {};
		const ecs::Entity& entity = objectComponentArray->entities[i];

		auto& object = objectComponentArray->components[i];
		auto& meshData = meshDataComponentArray->components[object.meshId];

		drawData.vertexBuffer = &meshData.vertexBuffer;
		drawData.indexBuffer = &meshData.indexBuffer;
		drawData.indexCount = meshData.GetNumIndices();

		TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
		drawData.worldTransform = transform->world;
		out.push_back(std::move(drawData));
	}
}

ecs::Entity Scene::CreateCube(std::string_view name)
{
	/*
	Logger::Debug("Transform: " + std::to_string(ecs::GetComponentTypeId<TransformComponent>()));
	Logger::Debug("Mesh: " + std::to_string(ecs::GetComponentTypeId<MeshDataComponent>()));
	Logger::Debug("Object: " + std::to_string(ecs::GetComponentTypeId<ObjectComponent>()));
	Logger::Debug("Name: " + std::to_string(ecs::GetComponentTypeId<NameComponent>()));
	*/
	// if invalid create base entity which can be used to instance other entity
	if (!mCubeEntity.IsValid())
	{
		mCubeEntity = ecs::CreateEntity();
		//mComponentManager->AddComponent<NameComponent>(cubeEntity).name = "cube";
		MeshDataComponent& meshComp = mComponentManager->AddComponent<MeshDataComponent>(mCubeEntity);

		meshComp.vertices = {
					Vertex{glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)},

					Vertex{glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)},

					Vertex{glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},

					Vertex{glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},

					Vertex{glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},

					Vertex{glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)},

					Vertex{glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)},

					Vertex{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)},
					Vertex{glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)},
		};

		meshComp.indices = {
			0,   1,  2,  0,  2,  3, // Top
			4,   5,  6,  4,  6,  7, // Front
			8,   9, 10,  8, 10, 11, // Right
			12, 13, 14, 12, 14, 15, // Left
			16, 17, 18, 16, 18, 19, // Back
			20, 22, 21, 20, 23, 22, // Bottom
		};
		meshComp.CreateRenderData();
	}

	ecs::Entity entity = ecs::CreateEntity();
	/*
	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;
		*/
	mComponentManager->AddComponent<TransformComponent>(entity);

	ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
	objectComp.meshId = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(mCubeEntity);
	return entity;
}

void Scene::Update(float dt)
{
	UpdateTransformData();
}

void Scene::Destroy(const ecs::Entity& entity)
{
	ecs::DestroyEntity(mComponentManager.get(), entity);
}

void Scene::UpdateTransformData()
{
	auto transforms = mComponentManager->GetComponentArray<TransformComponent>();

	for (auto& transform : transforms->components)
	{
		if (transform.dirty)
			transform.CalculateWorldMatrix();
	}
}
