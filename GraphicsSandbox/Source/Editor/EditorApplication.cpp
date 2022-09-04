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

	auto compMgr = mScene.GetComponentManager();
	for (auto light : lights)
	{
		auto transform = compMgr->GetComponent<TransformComponent>(light);
		glm::mat4 rotate = glm::yawPitchRoll(mDeltaTime * 0.001f, 0.0f, 0.0f) * transform->localMatrix;
		transform->localMatrix = rotate;
	}
}

void EditorApplication::PostUpdate(float dt) {

}

void EditorApplication::InitializeScene()

{
	auto compMgr = mScene.GetComponentManager();

	{
		ecs::Entity mesh = mScene.CreateMesh("Assets/Models/scene.sbox");
		if (mesh != ecs::INVALID_ENTITY)
		{
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(mesh);
			transform->scale = glm::vec3(1.0f);
			transform->position.y -= 1.0f;

			std::vector<ecs::Entity> children = mScene.FindChildren(mesh);
			if (children.size() > 0)
			{
				std::for_each(children.begin(), children.end(), [&compMgr](ecs::Entity child) {
					MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(child);
					material.roughness = MathUtils::Rand01();
					material.albedo = glm::vec4(0.944f, .776f, .373f, 1.0f);
					material.metallic = MathUtils::Rand01();
				 });
			}
			else {
				MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(mesh);
				material.roughness = 0.1f;
				material.albedo = glm::vec4(0.944f, .776f, .373f, 1.0f);
				material.metallic = 0.9f;
			}
		}
	}

	{
		ecs::Entity bloom = mScene.CreateMesh("Assets/Models/bloom.sbox");
		if (bloom != ecs::INVALID_ENTITY)
		{
			TransformComponent* transform = compMgr->GetComponent<TransformComponent>(bloom);
			transform->scale = glm::vec3(2.0f);
			transform->position = glm::vec3(0.0f, -1.0f, -3.0f);

			MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(bloom);
			material.roughness = 0.9f;
			material.albedo = glm::vec4(0.37f, .26f, .95f, 1.0f);
			material.metallic = 0.1f;
			material.emissive = 20.0f;
		}
	}


	mCamera->SetPosition({ 0.0f, 2.0f, 10.0f });
	mCamera->SetRotation({ 0.0f, glm::pi<float>(), 0.0f });
	
	glm::vec3 positions[] = {
		glm::vec3(-5.0f, 3.0f, 5.0f),
		glm::vec3(5.0f, 3.0f, 5.0f),
		glm::vec3(-5.0f, 3.0f, -5.0f),
		glm::vec3(5.0f, 3.0f, -5.0f)
	};

	glm::vec3 colors[] = {
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f)
	};

	for (int i = 0; i < 4; ++i)
	{
		ecs::Entity lightEntity = ecs::CreateEntity();
		TransformComponent& transform = compMgr->AddComponent<TransformComponent>(lightEntity);
		transform.position = positions[i];
		LightComponent& light = compMgr->AddComponent<LightComponent>(lightEntity);
		light.color = colors[i];
		light.intensity = 30.0f;
		light.type = LightType::Point;
		lights.push_back(lightEntity);
	}

	for(int i = 0; i < 4; ++i)
	{
		ecs::Entity cube = mScene.CreateCube("LightCube");
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(cube);
		transform->scale = glm::vec3(0.1f);
		transform->position = positions[i];
		MaterialComponent& material = compMgr->AddComponent<MaterialComponent>(cube);
		material.roughness = 1.0f;
		material.albedo = glm::vec4(colors[i], 1.0f);
		material.metallic = 0.2f;
		material.emissive = 30.0f;

		compMgr->AddComponent<HierarchyComponent>(cube).parent = lights[i];
		compMgr->AddComponent<HierarchyComponent>(lights[i]).childrens.push_back(cube);
	}

	/*
	{
		ecs::Entity cube = mScene.CreateCube("TestCube");
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(cube);
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
	*/
}

EditorApplication::~EditorApplication()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
