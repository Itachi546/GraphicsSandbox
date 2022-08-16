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

		InitializePrimitiveScene();
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

	void InitializePBRTestScene()
	{
		const float nrRows = 7;
		const float nrColumns = 7;
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

		mCamera = mScene.GetCamera();
		mCamera->SetPosition({ 0.0f, 5.0f, 10.0f });
		mCamera->SetRotation({ 0.0f, glm::pi<float>(), 0.0f });
	}

	void InitializePrimitiveScene()
	{
		auto compMgr = mScene.GetComponentManager();

		ecs::Entity cube = mScene.CreateCube("TestCube");
		TransformComponent* cT = compMgr->GetComponent<TransformComponent>(cube);
		MaterialComponent& cM = compMgr->AddComponent<MaterialComponent>(cube);
		cM.roughness = 0.3f;
		cM.albedo = glm::vec4(1.0f);
		cM.metallic = 0.3f;

		ecs::Entity sphere = mScene.CreateSphere("TestSphere");
		TransformComponent* sT = compMgr->GetComponent<TransformComponent>(sphere);
		sT->position.x += 2.0f;
		MaterialComponent& sM = compMgr->AddComponent<MaterialComponent>(sphere);
		sM.roughness = 0.3f;
		sM.albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
		sM.metallic = 0.7f;

		ecs::Entity plane = mScene.CreatePlane("TestPlane");
		TransformComponent* pT = compMgr->GetComponent<TransformComponent>(plane);
		pT->scale = glm::vec3(5.0f);
		pT->position.y -= 1.0f;
		MaterialComponent& pM = compMgr->AddComponent<MaterialComponent>(plane);
		pM.roughness = 0.1f;
		pM.albedo = glm::vec4(1.0f);
		pM.metallic = 0.5f;

		mCamera->SetPosition({ 0.0f, 5.0f, 10.0f });
		mCamera->SetRotation({ 0.0f, glm::pi<float>(), 0.0f });
	}
};
