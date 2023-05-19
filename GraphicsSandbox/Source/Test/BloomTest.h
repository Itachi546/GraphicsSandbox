#pragma once

#include "../Engine/GraphicsSandbox.h"

class BloomTest : public Application
{
public:
	BloomTest() = default;

	void Initialize() override
	{
		initialize_();
		mCamera = mScene.GetCamera();
		//mRenderer->SetEnableBloom(true);
		InitializeScene();
	}

	void PreUpdate(float dt) override {

		float walkSpeed = 0.5f;

		if (Input::Down(Input::Key::KEY_LEFT_SHIFT))
			walkSpeed *= 6.0f;

		if (Input::Down(Input::Key::KEY_ESCAPE))
			bRunning = false;

		if (Input::Down(Input::Key::KEY_W))
			mCamera->Walk(-dt * walkSpeed);
		else if (Input::Down(Input::Key::KEY_S))
			mCamera->Walk(dt * walkSpeed);
		if (Input::Down(Input::Key::KEY_A))
			mCamera->Strafe(-dt * walkSpeed);
		else if (Input::Down(Input::Key::KEY_D))
			mCamera->Strafe(dt * walkSpeed);
		if (Input::Down(Input::Key::KEY_1))
			mCamera->Lift(dt * walkSpeed);
		else if (Input::Down(Input::Key::KEY_2))
			mCamera->Lift(-dt * walkSpeed);

		if (Input::Down(Input::Key::MOUSE_BUTTON_LEFT))
		{
			auto [x, y] = Input::GetMouseState().delta;
			mCamera->Rotate(-y, x, dt);
		}
	}

	void PostUpdate(float dt) override {

	}

private:
	Camera* mCamera;

	void InitializeScene()
	{
		auto compMgr = mScene.GetComponentManager();
		{
			ecs::Entity cube = mScene.CreateCube("TestCube");
			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(cube);
			material.roughness = 0.01f;
			material.albedo = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
			material.metallic = 1.0f;
		}
		{
			ecs::Entity cube = mScene.CreateCube("TestCube");
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(cube);
			transform->position = glm::vec3(-2.5f, 1.0f, 0.0f);
			transform->scale = glm::vec3(1.0f, 2.0f, 1.0f);
			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(cube);
			material.roughness = 1.0f;
			material.albedo = glm::vec4(1.0f);
			material.metallic = 0.2f;
		}
		{
			ecs::Entity sphere = mScene.CreateSphere("TestSphere");
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(sphere);
			transform->position.x += 2.5f;
			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(sphere);
			material.roughness = 0.9f;
			material.albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			material.metallic = 0.1f;
			material.ao = 0.3f;
		}
		{
			ecs::Entity plane = mScene.CreatePlane("TestPlane");
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(plane);
			transform->scale = glm::vec3(40.0f);
			transform->position.y -= 1.0f;
			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(plane);
			material.roughness = 1.0f;
			material.albedo = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
			material.metallic = 0.0f;
			material.ao = 0.3f;
		}

		{
			ecs::Entity mesh = mScene.CreateMesh("Assets/Models/suzanne.sbox");
			if (mesh != ecs::INVALID_ENTITY)
			{
				TransformComponent* transform = compMgr->GetComponent<TransformComponent>(mesh);
				transform->scale = glm::vec3(1.0f);
				transform->position.x += 5.0f;
				transform->position.y -= 1.0f;

				MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(mesh);
				material.roughness = 0.5f;
				material.albedo = glm::vec4(0.944f, .776f, .373f, 1.0f);
				material.metallic = 1.0f;
				material.ao = 0.3f;
			}
		}
		{
			ecs::Entity bloom = mScene.CreateMesh("Assets/Models/bloom.sbox");
			if (bloom != ecs::INVALID_ENTITY)
			{
				TransformComponent* transform = compMgr->GetComponent<TransformComponent>(bloom);
				transform->scale = glm::vec3(1.0f);
				transform->position = glm::vec3(-6.0f, -0.5f, 0.0f);

				MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(bloom);
				material.roughness = 0.9f;
				material.albedo = glm::vec4(0.37f, .26f, .95f, 1.0f);
				material.metallic = 0.1f;
				material.emissive = glm::vec3(20.0f);
			}
		}


		mCamera->SetPosition({ 0.0f, 2.0f, 10.0f });
		mCamera->SetRotation({ 0.0f, glm::pi<float>(), 0.0f });

		glm::vec3 lightPosition = glm::vec3(0.0f, 2.0f, 5.0f);
		glm::vec3 lightColor = glm::vec3(0.944f, .776f, .373f);
		{
			ecs::Entity light = mScene.CreateLight("light1");
			TransformComponent* lightTransform = compMgr->GetComponent<TransformComponent>(light);
			lightTransform->position = lightPosition;
			LightComponent* lightComponent = compMgr->GetComponent<LightComponent>(light);
			lightComponent->color = lightColor;
			lightComponent->intensity = 5.0f;
			lightComponent->type = LightType::Point;
		}
		{
			ecs::Entity lightCube = mScene.CreateCube("lightCube");
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(lightCube);
			transform->position = lightPosition;
			transform->scale = glm::vec3(0.1f);
			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(lightCube);
			material.roughness = 1.0f;
			material.albedo = glm::vec4(lightColor, 1.0f);
			material.metallic = 0.0f;
			material.emissive = glm::vec3(10.0f);
		}

		mScene.SetEnableFrustumCulling(false);
		mScene.SetShowBoundingBox(false);
		DebugDraw::SetEnable(false);
	}
};
