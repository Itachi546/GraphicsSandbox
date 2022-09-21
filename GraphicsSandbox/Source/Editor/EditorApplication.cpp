#include "EditorApplication.h"
#include "../Engine/VulkanGraphicsDevice.h"
#include "../Engine/FX/Bloom.h"
#include "../Shared/MathUtils.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_vulkan.h"

#include <iomanip>
#include <algorithm>

void EditorApplication::Initialize()
{
	initialize_();
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui_ImplGlfw_InitForVulkan(mWindow, true);

	auto vkDevice = std::static_pointer_cast<gfx::VulkanGraphicsDevice>(mDevice);
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = vkDevice->GetInstance();
	initInfo.PhysicalDevice = vkDevice->GetPhysicalDevice();
	initInfo.Device = vkDevice->GetDevice();
	initInfo.Queue = vkDevice->GetQueue();
	initInfo.MinImageCount = vkDevice->GetSwapchainImageCount();
	initInfo.ImageCount = vkDevice->GetSwapchainImageCount();
	initInfo.DescriptorPool = vkDevice->GetDescriptorPool();
	initInfo.Allocator = 0;

	ImGui_ImplVulkan_Init(&initInfo, vkDevice->Get(mSwapchainRP.get()));

	gfx::CommandList commandList = mDevice->BeginCommandList();
	ImGui_ImplVulkan_CreateFontsTexture(vkDevice->Get(&commandList));
	mDevice->SubmitCommandList(&commandList);
	mDevice->WaitForGPU();
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	mCamera = mScene.GetCamera();
	InitializeScene();

	mHierarchy = std::make_shared<SceneHierarchy>(&mScene);
}

void EditorApplication::RenderUI(gfx::CommandList* commandList)
{
	static bool enableBloom = true;
	mRenderer->SetEnableBloom(enableBloom);
	auto vkDevice = std::static_pointer_cast<gfx::VulkanGraphicsDevice>(mDevice);
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::Begin("Statistics", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
	// Profilter Data
	std::vector<Profiler::RangeData> profilerData;
	Profiler::GetEntries(profilerData);
	if (profilerData.size() > 0)
	{
		std::stringstream ss;
		for (auto& data : profilerData)
		{
			if(data.inUse)
				ss << data.name << " " << std::setprecision(3) << data.time << "ms\n";
		}
		ImGui::Text("%s", ss.str().c_str());
	}
	ImGui::End();

	if (mShowUI)
	{
		ImGui::Begin("Render Settings");
		static bool showBoundingBox = mScene.GetShowBoundingBox();
		if (ImGui::Checkbox("Show BoundingBox", &showBoundingBox))
			mScene.SetShowBoundingBox(showBoundingBox);

		static bool enableFrustumCulling = true;
		ImGui::Checkbox("Frustum Culling", &enableFrustumCulling);
		mScene.SetEnableFrustumCulling(enableFrustumCulling);


		static bool freezeFrustum = false;
		ImGui::Checkbox("Freeze Frustum", &freezeFrustum);
		mScene.GetCamera()->SetFreezeFrustum(freezeFrustum);



		if (ImGui::CollapsingHeader("Bloom"))
		{
			ImGui::Checkbox("Enable", &enableBloom);
			static float blurRadius = 10.0f;
			if (ImGui::SliderFloat("BlurRadius", &blurRadius, 1.0f, 100.0f))
				mRenderer->SetBlurRadius(blurRadius);

			static float bloomThreshold = 1.0f;
			if (ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.1f, 2.0f))
				mRenderer->SetBloomThreshold(bloomThreshold);

			static float bloomStrength = 0.4f;
			if (ImGui::SliderFloat("Bloom Strength", &bloomStrength, 0.0f, 1.0f))
				mRenderer->SetBloomStrength(bloomStrength);
		}
		ImGui::End();

		mHierarchy->Draw();
	}

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkDevice->Get(commandList));
}

void EditorApplication::PreUpdate(float dt) {

	float walkSpeed = 2.0f;

	if (Input::Down(Input::Key::KEY_LEFT_SHIFT))
		walkSpeed *= 5.0f;

	if (Input::Down(Input::Key::KEY_ESCAPE))
		bRunning = false;

	if (Input::Down(Input::Key::KEY_W))
		mCamera->Walk(dt * walkSpeed);
	else if (Input::Down(Input::Key::KEY_S))
		mCamera->Walk(-dt * walkSpeed);
	if (Input::Down(Input::Key::KEY_A))
		mCamera->Strafe(-dt * walkSpeed);
	else if (Input::Down(Input::Key::KEY_D))
		mCamera->Strafe(dt * walkSpeed);
	if (Input::Down(Input::Key::KEY_1))
		mCamera->Lift(dt * walkSpeed);
	else if (Input::Down(Input::Key::KEY_2))
		mCamera->Lift(-dt * walkSpeed);

	if (Input::Down(Input::Key::MOUSE_BUTTON_LEFT) && 
		!ImGui::IsAnyItemActive() && 
		!ImGui::IsAnyItemHovered())
	{
		auto [x, y] = Input::GetMouseState().delta;
		mCamera->Rotate(-y, x, dt);
	}
	mRenderer->SetUpdateBatches(true);

	if (Input::Press(Input::Key::KEY_H))
		mShowUI = !mShowUI;
}

void EditorApplication::PostUpdate(float dt) {

}

void EditorApplication::InitializeScene()
{
	mCamera->SetPosition({ 6.0f, -4.0f, 0.0f });
	mCamera->SetRotation({ 0.0f, -glm::pi<float>() * 0.5, 0.0f });
	auto compMgr = mScene.GetComponentManager();
#if 1
	{
		ecs::Entity sponza = mScene.CreateMesh("Assets/Models/sponza.sbox");
		if (sponza != ecs::INVALID_ENTITY)
		{
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(sponza);
			transform->scale = glm::vec3(1.0f);
			transform->position.y -= 5.0f;
		}
		/*
		ecs::Entity curtains = mScene.CreateMesh("Assets/Models/NewSponza_IvyGrowth_gltf.sbox");
		if (curtains != ecs::INVALID_ENTITY)
		{
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(curtains);
			transform->scale = glm::vec3(1.0f);
			transform->position.y -= 5.0f;
		}
		*/
	}

	{
		ecs::Entity bloom = mScene.CreateMesh("Assets/Models/teapot.sbox");
		if (bloom != ecs::INVALID_ENTITY)
		{
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(bloom);
			transform->scale = glm::vec3(0.2f);
			transform->position = glm::vec3(2.0f, -5.0f, 0.0f);
			transform->rotation = glm::vec3(0.0f, -1.57f, 0.0f);
			MaterialComponent* material = compMgr->GetComponent<MaterialComponent>(bloom);
			material->emissive = 40.0f;
			material->albedo = glm::vec4(0.18f, 0.55f, 0.84f, 1.0f);
		}
	}
	{
		ecs::Entity sphere = mScene.CreateSphere("TestSphere");
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(sphere);
		transform->scale = glm::vec3(0.2f);
		transform->position.y = -4.8f;
		MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(sphere);
		material.roughness = 0.9f;
		material.albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
		material.metallic = 0.1f;
	}
#endif
	{
		ecs::Entity cube = mScene.CreateCube("TestCube");
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(cube);
		transform->scale = glm::vec3(0.2f, 0.6f, 0.2f);
		transform->position = glm::vec3(0.0f, -4.7f, 0.5f);
		MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(cube);
		material.roughness = 1.0f;
		material.albedo = glm::vec4(1.0f);
		material.metallic = 0.2f;
	}

}

EditorApplication::~EditorApplication()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
