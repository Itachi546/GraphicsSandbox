#include "EditorApplication.h"
#include "../Engine/VulkanGraphicsDevice.h"
#include "../Engine/FX/Bloom.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_vulkan.h"

#include <iomanip>

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
}

void EditorApplication::RenderUI(gfx::CommandList* commandList)
{
	auto vkDevice = std::static_pointer_cast<gfx::VulkanGraphicsDevice>(mDevice);
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::Begin("Statistics", 0, ImGuiWindowFlags_NoMove);

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

	static bool enableBloom = true;
	mRenderer->SetEnableBloom(enableBloom);
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

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkDevice->Get(commandList));
}

void EditorApplication::PreUpdate(float dt) {

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

	if (Input::Down(Input::Key::MOUSE_BUTTON_LEFT) && 
		!ImGui::IsAnyItemActive() && 
		!ImGui::IsAnyItemHovered())
	{
		auto [x, y] = Input::GetMouseState().delta;
		mCamera->Rotate(-y, x, dt);
	}
}

void EditorApplication::PostUpdate(float dt) {

}

void EditorApplication::InitializeScene()
{
	auto compMgr = mScene.GetComponentManager();
	{
		ecs::Entity mesh = mScene.CreateMesh("Assets/Models/suzanne.sbox");
		if (mesh != ecs::INVALID_ENTITY)
		{
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(mesh);
			transform->scale = glm::vec3(1.0f);
			transform->position.x += 5.0f;
			transform->position.y -= 1.0f;

			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(mesh);
			material.roughness = 0.1f;
			material.albedo = glm::vec4(0.944f, .776f, .373f, 1.0f);
			material.metallic = 0.9f;
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
			material.emissive = 20.0f;
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
		material.emissive = 10.0f;
	}

}

EditorApplication::~EditorApplication()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
