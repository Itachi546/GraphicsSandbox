#include "EditorApplication.h"
#include "../Engine/VulkanGraphicsDevice.h"
#include "../Shared/MathUtils.h"
#include "../Engine/DebugDraw.h"
#include "../Engine/CascadedShadowMap.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "ImPlot/implot.h"
#include "ImGui/IconsFontAwesome5.h"

#include "TransformGizmo.h"
#include "../Engine/Animation/Animation.h"
#include "../Engine/Interpolator.h"
#include <iomanip>
#include <algorithm>

void EditorApplication::Initialize()
{
	initialize_();
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	mWidth = io.DisplaySize.x;
	mHeight = io.DisplaySize.y;

	io.Fonts->AddFontDefault();
	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
	ImFontConfig iconConfig; 
	iconConfig.MergeMode = true;
	iconConfig.PixelSnapH = true;
	iconConfig.GlyphMinAdvanceX = 13.0f;
	std::string filename = "Assets/Fonts/" + std::string(FONT_ICON_FILE_NAME_FAS);
	io.Fonts->AddFontFromFileTTF(filename.c_str(), 13.0f, &iconConfig, icons_ranges);
	// use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid

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

	ImGui_ImplVulkan_Init(&initInfo, vkDevice->Get(mSwapchainRP));

	gfx::CommandList commandList = mDevice->BeginCommandList();
	ImGui_ImplVulkan_CreateFontsTexture(vkDevice->Get(&commandList));
	mDevice->SubmitCommandList(&commandList);
	mDevice->WaitForGPU();
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	mCamera = mScene.GetCamera();
	InitializeScene();

	mHierarchy = std::make_shared<SceneHierarchy>(&mScene, mWindow);
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
	static bool enableFrustumCulling = false;
	static bool freezeFrustum = false;
	static bool showBoundingBox = mScene.GetShowBoundingBox();
	static float blurRadius = 1.0f;
	static float bloomThreshold = 1.0f;
	static float bloomStrength = 0.04f;
	static bool enableNormalMapping = true;
	static float shadowSplitLambda = 0.85f;
	static float shadowDistance = 150.0f;
	static bool showCascade = false;
	if (mShowUI)
	{
		ImGui::Begin("Render Settings");
		ImGui::Checkbox("Show BoundingBox", &showBoundingBox);
		ImGui::Checkbox("Show Skeleton", &mShowSkeleton);
		ImGui::Checkbox("Show Cascade", &showCascade);
		ImGui::Checkbox("Debug Draw Enabled", &enableDebugDraw);
		ImGui::Checkbox("Normal Mapping", &enableNormalMapping);
		ImGui::Checkbox("Frustum Culling", &enableFrustumCulling);
		ImGui::Checkbox("Freeze Frustum", &freezeFrustum);

		if (ImGui::CollapsingHeader("Bloom"))
		{
			ImGui::Checkbox("Enable", &enableBloom);
			ImGui::SliderFloat("BlurRadius", &blurRadius, 1.0f, 100.0f);
			ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.1f, 2.0f);
			ImGui::SliderFloat("Bloom Strength", &bloomStrength, 0.0f, 1.0f);
		}
		if (ImGui::CollapsingHeader("Cascaded ShadowMap"))
		{
			ImGui::SliderFloat("SplitLambda", &shadowSplitLambda, 0.0f, 1.0f);
			ImGui::SliderFloat("ShadowDistance", &shadowDistance, 5.0f, 500.0f);
		}

		if (ImGui::CollapsingHeader("Interpolators"))
		{
			static glm::dvec2 a(0.0f, 0.0f);
			static glm::dvec2 b(1.0f, 1.0f);
			static glm::dvec2 c1(1.0f, 0.0f);
			static glm::dvec2 c2(1.0f, 0.0f);

			std::vector<double> bezierX;
			std::vector<double> bezierY;
			for (int i = 0; i < 80; ++i)
			{
				double x = Interpolator::BezierSpline<double>(a.x, b.x, c1.x, c2.x, double(i) / 80.0);
				double y = Interpolator::BezierSpline<double>(a.y, b.y, c1.y, c2.y, double(i) / 80.0);
				bezierX.push_back(x);
				bezierY.push_back(y);
			}
			if (ImPlot::BeginPlot("Curve"))
			{
				double v1x[] = { a.x, c1.x };
				double v1y[] = { a.y, c1.y };
				ImPlot::PlotLine("line", v1x, v1y, 2);

				double v2x[] = { b.x, c2.x };
				double v2y[] = { b.y, c2.y };
				ImPlot::PlotLine("line", v2x, v2y, 2);

				ImPlot::DragPoint(2, &c1.x, &c1.y, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				ImPlot::DragPoint(3, &c2.x, &c2.y, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

				ImPlot::PlotLine("Curve", bezierX.data(), bezierY.data(), (int)bezierX.size());
				ImPlot::EndPlot();
			}
		}

		ImGui::End();
		mHierarchy->Draw();
	}
	auto shadowMap = mRenderer->GetShadowMap();
	shadowMap->SetSplitLambda(shadowSplitLambda);
	shadowMap->SetShadowDistance(shadowDistance);
	mScene.SetEnableFrustumCulling(enableFrustumCulling);
	mScene.GetCamera()->SetFreezeFrustum(freezeFrustum);
	mRenderer->SetEnableNormalMapping(enableNormalMapping);
	mRenderer->SetBlurRadius(blurRadius);
	mRenderer->SetBloomThreshold(bloomThreshold);
	mRenderer->SetBloomStrength(bloomStrength);
	mRenderer->SetDebugCascade(showCascade);
	mScene.SetShowBoundingBox(showBoundingBox);
	DebugDraw::SetEnable(enableDebugDraw);


	if (mShowSkeleton && enableDebugDraw)
		DrawSkeleton();

	//TransformGizmo::BeginFrame();
	//static glm::mat4 out;
	//TransformGizmo::Manipulate(mCamera->GetViewMatrix(), mCamera->GetProjectionMatrix(), TransformGizmo::Operation::Translate, out);


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

	if (Input::Press(Input::Key::KEY_H))
		mShowUI = !mShowUI;

	mHierarchy->Update(dt);
	mCurrentTime += dt;

	auto compMgr = mScene.GetComponentManager();
	auto skinnedMeshRenderers = compMgr->GetComponentArray<SkinnedMeshRenderer>();
	if (Input::Down(Input::Key::KEY_F)) {
		mCurrentTime = 0.0f;
	}

	for (uint32_t i = 0; i < skinnedMeshRenderers->GetCount(); ++i)
	{
		const ecs::Entity entity = skinnedMeshRenderers->entities[i];
		SkinnedMeshRenderer& meshRenderer = skinnedMeshRenderers->components[i];
		std::vector<AnimationClip> animationClips = meshRenderer.animationClips;
		TransformComponent* transform = mScene.GetComponentManager()->GetComponent<TransformComponent>(entity);

		AnimationClip& animationClip = meshRenderer.animationClips[0];

		Skeleton& skeleton = meshRenderer.skeleton;
		Pose& animatedPose = skeleton.GetAnimatedPose();
		// @TODO: Remove explicit copy method 
		PoseMode poseMode = skeleton.GetPoseMode();
		if (poseMode == PoseMode::Rest)
			animatedPose.Copy(skeleton.GetRestPose());
		else if (poseMode == PoseMode::Bind)
			animatedPose.Copy(skeleton.GetBindPose());
		else if (poseMode == PoseMode::Animation)
		{
			animatedPose.SetDirty(true);
			animationClip.Sample(animatedPose, mCurrentTime * animationClip.GetTickPerSeconds());
		}
	}
}

void EditorApplication::PostUpdate(float dt) {

}

void EditorApplication::InitializeScene()
{
	mCamera->SetPosition({ 5.0f, 1.0f, 5.0f });
	mCamera->SetRotation({ 0.0f, -glm::pi<float>() * 0.5f, 0.0f });
	mCamera->SetNearPlane(0.1f);
	mCamera->SetFarPlane(1000.0f);
	mScene.SetEnableFrustumCulling(false);

	auto compMgr = mScene.GetComponentManager();

	//character = mScene.CreateMesh("Assets/Models/character2.sbox");

	character = mScene.CreateMesh("Assets/Models/michelle.sbox");
	compMgr->GetComponent<TransformComponent>(character)->position = glm::vec3(5.0f, 0.0f, 6.0f);
	ecs::Entity ortiz = mScene.CreateMesh("Assets/Models/ortiz.sbox");

	ecs::Entity plane = mScene.CreatePlane("Plane00");
	compMgr->GetComponent<TransformComponent>(plane)->scale = glm::vec3(30.0f);

	{
		ecs::Entity helmet = mScene.CreateMesh("Assets/Models/DamagedHelmet.sbox");
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(helmet);
		transform->position.y += 0.5f;
		transform->position.x += 1.0f;
		transform->scale *= 0.5f; 
		transform->rotation = glm::quat(glm::vec3(0.0, glm::pi<float>() * 0.5, 0.0f));
	}
}

void EditorApplication::InitializeCSMScene()
{
	mCamera->SetPosition({ 5.0f, 1.0f, 5.0f });
	mCamera->SetRotation({ 0.0f, -glm::pi<float>() * 0.5f, 0.0f });
	mCamera->SetNearPlane(0.1f);
	mCamera->SetFarPlane(1000.0f);
	mScene.SetEnableFrustumCulling(false);

	auto compMgr = mScene.GetComponentManager();

	const int kNumObjects = 10;
	for (int i = 0; i < kNumObjects; ++i)
	{
		ecs::Entity cube = mScene.CreateCube("cube" + std::to_string(i));
		TransformComponent* comp = compMgr->GetComponent<TransformComponent>(cube);
		comp->position.x += ((kNumObjects * 0.5f) - i) * 4.0f;
		comp->position.y += 2.0f;
		comp->scale.y = 2.0f;
	}

	ecs::Entity plane = mScene.CreatePlane("Plane");
	TransformComponent* comp = compMgr->GetComponent<TransformComponent>(plane);
	comp->scale = glm::vec3(40.0f);
}

void EditorApplication::DrawPose(Skeleton& skeleton, ImDrawList* drawList, const glm::mat4& worldTransform)
{

	Pose& animatedPose = skeleton.GetAnimatedPose();
	uint32_t count = animatedPose.GetSize();
	auto camera = mScene.GetCamera();
	static const uint32_t color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
	for (uint32_t i = 0; i < count; ++i)
	{
		int parent = animatedPose.GetParent(i);
		if (parent == -1) continue;

		glm::mat4 parentTransform = animatedPose.GetGlobalTransform(parent).CalculateWorldMatrix();
		glm::mat4 currentTransform = animatedPose.GetGlobalTransform(i).CalculateWorldMatrix();

		glm::vec3 parentBonePosition = worldTransform * parentTransform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		glm::vec3 currentBonePosition = worldTransform * currentTransform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		glm::vec4 p0 = camera->ComputeNDCCoordinate(parentBonePosition); //*glm::vec2(mWidth, mHeight);
		glm::vec4 p1 = camera->ComputeNDCCoordinate(currentBonePosition);// *glm::vec2(mWidth, mHeight);

		if (p0.z > 1.0f || p0.z < -1.0 || p1.z > 1.0f || p1.z < -1.0f) continue;
		drawList->AddLine(ImVec2(p0.x * mWidth, p0.y * mHeight), ImVec2(p1.x * mWidth, p1.y * mHeight), color, 2.0f);
	}

}

void EditorApplication::DrawSkeleton()
{
	auto compMgr = mScene.GetComponentManager();
	auto skinnedMeshRenderers = compMgr->GetComponentArray<SkinnedMeshRenderer>();

	const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGuiIO& io = ImGui::GetIO();
	mWidth = io.DisplaySize.x;
	mHeight = io.DisplaySize.y;

	ImGui::SetWindowSize(io.DisplaySize);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
	ImGui::PushStyleColor(ImGuiCol_Border, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

	ImGui::Begin("Skeleton", NULL, flags);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);

	for (uint32_t i = 0; i < skinnedMeshRenderers->GetCount(); ++i)
	{
		const ecs::Entity entity = skinnedMeshRenderers->entities[i];
		const glm::mat4& worldTransform = compMgr->GetComponent<TransformComponent>(entity)->CalculateWorldMatrix();
		SkinnedMeshRenderer& meshRenderer = skinnedMeshRenderers->components[i];
		DrawPose(meshRenderer.skeleton, drawList, worldTransform);
	}
}

EditorApplication::~EditorApplication()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
}
