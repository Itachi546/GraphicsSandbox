#pragma once

#include "../Engine/Application.h"
#include "../Engine/VulkanGraphicsDevice.h"
#include "../Shared/MathUtils.h"
#include "../Engine/DebugDraw.h"
#include "../Engine/CascadedShadowMap.h"
#include "../Engine/Input.h"

#include <iomanip>
#include <algorithm>

class SponzaTest : public Application
{
	void Initialize()
	{
		initialize_();
		mCamera = mScene.GetCamera();
		InitializeScene();
	}

	void PreUpdate(float dt) {

		float walkSpeed = 2.0f;

		if (Input::Down(Input::Key::KEY_LEFT_SHIFT))
			walkSpeed *= 5.0f;

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

	void InitializeScene()
	{
		mCamera->SetPosition({ 5.0f, 1.0f, 0.0f });
		mCamera->SetRotation({ 0.0f, -glm::pi<float>() * 0.5f, 0.0f });
		mCamera->SetNearPlane(0.1f);
		mCamera->SetFarPlane(1000.0f);
		mScene.SetEnableFrustumCulling(false);
		mScene.SetShowBoundingBox(false);
		DebugDraw::SetEnable(false);
		mRenderer->GetShadowMap()->SetShadowDistance(150.0f);
		mRenderer->GetShadowMap()->SetSplitLambda(0.85f);


		auto compMgr = mScene.GetComponentManager();

		ecs::Entity scene = mScene.CreateMesh("Assets/Models/sponza.sbox");
		ecs::Entity sphere = mScene.CreateSphere("Sphere00");

		compMgr->GetComponent<TransformComponent>(scene)->scale = glm::vec3(1.0f);
		compMgr->GetComponent<TransformComponent>(sphere)->scale = glm::vec3(0.4f);
		compMgr->GetComponent<TransformComponent>(sphere)->position.y += 2.0f;
		compMgr->GetComponent<MaterialComponent>(sphere)->albedo = glm::vec4(0.01f, 0.235f, 1.0f, 1.0f);

		// Set Sun
		compMgr->GetComponent<TransformComponent>(mScene.GetSun())->rotation = glm::vec3(0.1f, 1.0f, 0.0f);
	}

	private:
		Camera* mCamera;
};
