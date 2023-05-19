#include "EditorApplication.h"
#include "../Engine/VulkanGraphicsDevice.h"
#include "../Shared/MathUtils.h"
#include "../Engine/DebugDraw.h"
#include "../Engine/CascadedShadowMap.h"
#include "../Engine/Animation/Animation.h"
#include "../Engine/Interpolator.h"
#include "../Engine/GUI/ImGuiService.h"

#include "TransformGizmo.h"
#include <iomanip>
#include <algorithm>

void EditorApplication::Initialize()
{
	initialize_();

	WindowProperties props;
	Platform::GetWindowProperties(mWindow, &props);
	mWidth = (float)props.width;
	mHeight = (float)props.height;
	mCamera = mScene.GetCamera();
	InitializeScene();
	mHierarchy = std::make_shared<SceneHierarchy>(&mScene, mWindow);
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

	if (Input::Down(Input::Key::MOUSE_BUTTON_LEFT) && !mGuiService->IsAcceptingEvent())
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
	/*
	character = mScene.CreateMesh("Assets/Models/michelle.sbox");
	compMgr->GetComponent<TransformComponent>(character)->position = glm::vec3(5.0f, 0.0f, 6.0f);
	ecs::Entity ortiz = mScene.CreateMesh("Assets/Models/ortiz.sbox");
	*/

	ecs::Entity scene = mScene.CreateMesh("Assets/Models/sponza.sbox");

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
/*
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
*/
EditorApplication::~EditorApplication()
{
	mDevice->WaitForGPU();
}
