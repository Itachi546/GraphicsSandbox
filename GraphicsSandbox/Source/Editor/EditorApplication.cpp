#include "EditorApplication.h"
#include "../Engine/VulkanGraphicsDevice.h"
#include "../Shared/MathUtils.h"
#include "../Engine/DebugDraw.h"

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

	static bool enableDebugDraw = false;
	static bool enableFrustumCulling = true;
	static bool freezeFrustum = false;
	static bool showBoundingBox = mScene.GetShowBoundingBox();
	static float blurRadius = 10.0f;
	static float bloomThreshold = 1.0f;
	static float bloomStrength = 0.04f;
	static bool enableNormalMapping = true;
	if (mShowUI)
	{
		ImGui::Begin("Render Settings");
		ImGui::Checkbox("Show BoundingBox", &showBoundingBox);
		ImGui::Checkbox("Debug Draw Enabled", &enableDebugDraw);
		ImGui::Checkbox("Normal Mapping", &enableNormalMapping);
		ImGui::Checkbox("Frustum Culling", &enableFrustumCulling);
		mScene.SetEnableFrustumCulling(enableFrustumCulling);
		ImGui::Checkbox("Freeze Frustum", &freezeFrustum);
		mScene.GetCamera()->SetFreezeFrustum(freezeFrustum);
		if (ImGui::CollapsingHeader("Bloom"))
		{
			ImGui::Checkbox("Enable", &enableBloom);
			ImGui::SliderFloat("BlurRadius", &blurRadius, 1.0f, 100.0f);
			ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.1f, 2.0f);
			ImGui::SliderFloat("Bloom Strength", &bloomStrength, 0.0f, 1.0f);
		}
		ImGui::End();
		mHierarchy->Draw();
	}
	mRenderer->SetEnableNormalMapping(enableNormalMapping);
	mRenderer->SetBlurRadius(blurRadius);
	mRenderer->SetBloomThreshold(bloomThreshold);
	mRenderer->SetBloomStrength(bloomStrength);
	mScene.SetShowBoundingBox(showBoundingBox);
	DebugDraw::SetEnable(enableDebugDraw);

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
	mRenderer->SetUpdateBatches(true);

	if (Input::Press(Input::Key::KEY_H))
		mShowUI = !mShowUI;
}

void EditorApplication::PostUpdate(float dt) {

}

void EditorApplication::InitializeScene()
{
	mCamera->SetPosition({ 0.0f, 1.0f, 3.0f });
	mCamera->SetRotation({ 0.0f, glm::pi<float>(), 0.0f });
	mCamera->SetNearPlane(0.3f);
	mCamera->SetFarPlane(3000.0f);
	mScene.SetEnableFrustumCulling(true);

	auto compMgr = mScene.GetComponentManager();

	ecs::Entity scene = mScene.CreateMesh("Assets/Models/scene.sbox");
	//ecs::Entity scene = mScene.CreateCube("Cube001");
}

EditorApplication::~EditorApplication()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
