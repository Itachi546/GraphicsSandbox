#pragma once

#include "../Engine/GraphicsSandbox.h"
#include "SceneHierarchy.h"

struct ImDrawList;

class EditorApplication : public Application
{
public:
	EditorApplication() = default;

	void Initialize() override;

	void PreUpdate(float dt) override;

	void PostUpdate(float dt) override;

	void RenderUI(gfx::CommandList* commandList) override;
	~EditorApplication();
private:
	Camera* mCamera;
	void InitializeScene();
	void InitializeCSMScene();
	void DrawPose(Skeleton& skeleton, ImDrawList* drawList, const glm::mat4& worldTransform);
	//void UpdateSkeletonTransform(Skeleton& skeleton, const AnimationClip& animationClip, uint32_t node, const glm::mat4& parentTransform);
	void DrawSkeleton();

	std::shared_ptr<SceneHierarchy> mHierarchy;
	ecs::Entity character;
	bool mShowUI = false;
	bool mShowSkeleton = true;

	float mCurrentTime = 0.0f;
	float mWidth, mHeight;
};
