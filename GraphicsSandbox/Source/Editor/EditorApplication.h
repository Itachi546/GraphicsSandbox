#pragma once

#include "../Engine/GraphicsSandbox.h"
#include "SceneHierarchy.h"

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

	std::shared_ptr<SceneHierarchy> mHierarchy;
	ecs::Entity character;
	bool mShowUI = false;
};
