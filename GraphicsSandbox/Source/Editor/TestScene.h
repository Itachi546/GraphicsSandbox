#pragma once

#include "../Engine/GraphicsSandbox.h"

void InitializeSponzaScene(Scene* scene) {
	Camera* camera = scene->GetCamera();
	camera->SetPosition({ 5.0f, 3.0f, 0.0f });
	camera->SetRotation({ 0.0f, -glm::pi<float>() * 0.5f, 0.0f });
	camera->SetNearPlane(0.2f);
	camera->SetFarPlane(500.0f);

	auto compMgr = scene->GetComponentManager();
	ecs::Entity e0 = scene->CreateMesh("C:/Users/Dell/OneDrive/Documents/3D-Assets/Models/sponza/sponza.gltf");
	{
		ecs::Entity e1 = scene->CreateMesh("C:/Users/Dell/OneDrive/Documents/3D-Assets/Models/damagedHelmet/damagedHelmet.gltf");
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(e1);
		transform->scale = glm::vec3(0.5f);
		transform->position += glm::vec3{3.0f, 1.0f, 0.0f};
	}
#if 1
	const int nLight = 10;
	const float lightStep = 20.0f / nLight;

	for (uint32_t i = 0; i < nLight; ++i)
	{
		ecs::Entity light = ecs::CreateEntity();
		compMgr->AddComponent<NameComponent>(light).name = "light" + std::to_string(i);
		TransformComponent& transform = compMgr->AddComponent<TransformComponent>(light);
		//transform.position = glm::vec3(MathUtils::Rand01() * 16.0f - 8.0f, 3.0f, 0.0f);
		transform.position = glm::vec3(-10.0f + i * lightStep, 2.0f, 0.0f);
		transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);

		LightComponent& lightComp = compMgr->AddComponent<LightComponent>(light);
		lightComp.color = glm::vec3(MathUtils::Rand01(), 1.0f - MathUtils::Rand01(), MathUtils::Rand01());
		lightComp.intensity = 2.0f;
		lightComp.type = LightType::Point;
		lightComp.radius = 5.0f;
	}
#endif
}

void InitializeAOScene(Scene* scene) {
	Camera* camera = scene->GetCamera();
	camera->SetPosition({ 5.0f, 3.0f, 0.0f });
	camera->SetRotation({ 0.0f, -glm::pi<float>() * 0.5f, 0.0f });
	camera->SetNearPlane(0.2f);
	camera->SetFarPlane(500.0f);

	auto compMgr = scene->GetComponentManager();
	ecs::Entity e0 = scene->CreateMesh("C:/Users/Dell/OneDrive/Documents/3D-Assets/Models/dragon/dragon.glb");
	ecs::Entity sphere = scene->CreateSphere("Sphere");
	compMgr->GetComponent<TransformComponent>(sphere)->position = glm::vec3(0.0f, 1.0f, 3.0f);
}


void InitializeGroundProjectedScene(Scene* scene) {
	Camera* camera = scene->GetCamera();
	camera->SetPosition({ 5.0f, 3.0f, 0.0f });
	camera->SetRotation({ 0.0f, -glm::pi<float>() * 0.5f, 0.0f });
	camera->SetNearPlane(0.2f);
	camera->SetFarPlane(500.0f);

	auto compMgr = scene->GetComponentManager();
	ecs::Entity e1 = scene->CreateMesh("C:/Users/Dell/OneDrive/Documents/3D-Assets/Models/damagedHelmet/damagedHelmet.gltf");
	TransformComponent* transform = compMgr->GetComponent<TransformComponent>(e1);
	transform->scale = glm::vec3(0.5f);
	transform->position += glm::vec3{ 3.0f, 1.0f, 0.0f };
}