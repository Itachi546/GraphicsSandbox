#pragma once

#include "CommonInclude.h"
#include "Platform.h"
#include "Timer.h"
#include "EventDispatcher.h"
#include "GraphicsDevice.h"
#include "GlmIncludes.h"
#include "ECS.h"
#include "Scene.h"
#include "Renderer.h"

#include <sstream>
#include <memory>

class Renderer;

namespace ui {
	struct ImGuiService;
};

class Application
{

public:
	virtual void Initialize() = 0;
	virtual void PreUpdate(float dt){}
	virtual void PostUpdate(float dt) {}

	void Run();

	void setTargetFrameRate(float value) { mTargetFrameRate = value; }

	float getTargetFrameRate() const { return mTargetFrameRate; }

	void setLockFrameRate(bool enabled) { mLockFrameRate = enabled; }

	void SetWindow(Platform::WindowType window, bool fullscreen);

	bool IsRunning() const { return bRunning; }

	virtual ~Application();

	static Platform::WindowType window;
protected:

	void initialize_();

	void update_(float dt);

	void render_();

	Timer mTimer;

	bool mInitialized = false;
	float mTargetFrameRate = 60.0f;
	bool mLockFrameRate = true;

	float mDeltaTime = 0.0f;
	float mElapsedTime = 0.0f;

	std::shared_ptr<gfx::GraphicsDevice > mDevice = nullptr;
	ui::ImGuiService* mGuiService = nullptr;

	std::unique_ptr<Renderer> mRenderer;

	std::stringstream mWindowTitle;

	Scene mScene;

	int mWidth, mHeight;

	bool bRunning = true;

	bool windowResizeEvent(const Event& event);
	
};