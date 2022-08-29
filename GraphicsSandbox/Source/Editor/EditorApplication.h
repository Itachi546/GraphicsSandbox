#pragma once

#include "../Engine/GraphicsSandbox.h"

class EditorApplication : public Application
{
public:
	EditorApplication() = default;

	void Initialize() override;

	void PreUpdate(float dt) override;

	void PostUpdate(float dt) override;
private:
	Camera* mCamera;

	void InitializeScene();
};
