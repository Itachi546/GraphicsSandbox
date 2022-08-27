#pragma once

#include "Engine/GraphicsSandbox.h"

class SandboxApplication : public Application
{
public:
	SandboxApplication() = default;

	void Initialize() override
	{
		initialize_();
		mCamera = mScene.GetCamera();

		//InitializeBloomScene();
		InitializePrimitiveScene();
		//InitializePBRTestScene();
	}

	void PreUpdate(float dt) override {

		float walkSpeed = 1.0f;

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

	void InitializeBloomScene()
	{
		auto compMgr = mScene.GetComponentManager();
		{
			ecs::Entity bloom = mScene.CreateMesh("Assets/Models/bloom.sbox");
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(bloom);
			transform->scale = glm::vec3(1.0f);

			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(bloom);
			material.roughness = 0.9f;
			material.albedo = glm::vec4(0.944f, .776f, .373f, 1.0f);
			material.metallic = 0.1f;
			material.emissive = glm::vec4(9.44f, 7.76f, 3.73f, 1.0f) * 2.0f;
		}
		{
			ecs::Entity cube = mScene.CreateCube("cube");
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(cube);
			transform->scale = glm::vec3(1.0f);
			transform->position = glm::vec3(2.0f, 0.0f, 2.0f);

			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(cube);
			material.roughness = 0.9f;
			material.albedo = glm::vec4(0.1f, 0.1f, 0.9f, 1.0f);
			material.metallic = 0.1f;
			material.emissive = glm::vec4(1.0f, 1.0f, 9.0f, 1.0f);

		}

		mCamera->SetPosition({ 0.0f, 0.5f, 2.0f });
		mCamera->SetRotation({ 0.0f, glm::pi<float>(), 0.0f });
	}

	void InitializePBRTestScene()
	{
		const float nrRows = 50;
		const float nrColumns = 50;
		const float spacing = 2.5f;
		auto compManager = mScene.GetComponentManager();
		for (int row = 0; row < nrRows; ++row)
		{
			float metallic = (float)row / (float)nrRows;
			for (int col = 0; col < nrColumns; ++col)
			{
				// we clamp the roughness to 0.025 - 1.0 as perfectly smooth surfaces (roughness of 0.0) tend to look a bit off
				// on direct lighting.
				float roughness = glm::clamp((float)col / (float)nrColumns, 0.05f, 1.0f);
				glm::vec3 position = glm::vec3(
					(float)(col - (nrColumns / 2)) * spacing,
					(float)(row - (nrRows / 2)) * spacing,
					-2.0f
				);

				ecs::Entity entity = mScene.CreateSphere("sphere");
				TransformComponent* transform = compManager->GetComponent<TransformComponent>(entity);
				transform->position = position;

				MaterialComponent& mat = compManager->AddComponent<MaterialComponent>(entity);
				mat.albedo = glm::vec4(0.5f, 0.0f, 0.0f, 1.0f);
				mat.ao = 1.0f;
				mat.roughness = roughness;
				mat.metallic = metallic;
			}
		}

		mCamera->SetPosition({ 0.0f, 5.0f, 10.0f });
		mCamera->SetRotation({ 0.0f, glm::pi<float>(), 0.0f });
	}

	void InitializePrimitiveScene()
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
		}

		{
			ecs::Entity mesh = mScene.CreateMesh("Assets/Models/suzanne.sbox");
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(mesh);
			transform->scale = glm::vec3(1.0f);
			transform->position.x += 5.0f;
			transform->position.y -= 1.0f;

			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(mesh);
			material.roughness = 0.1f;
			material.albedo = glm::vec4(0.944f, .776f, .373f, 1.0f);
			material.metallic = 0.9f;
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
			lightComponent->intensity = 50.0f;
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
			material.emissive = glm::vec4(lightColor * 10.0f, 1.0f);
		}

	}
};
