#include "Application.h"
#include "Input.h"
#include "Logger.h"
#include "Utils.h"
#include "VulkanGraphicsDevice.h"
#include "Scene.h"
#include "Profiler.h"
#include "TextureCache.h"
#include "DebugDraw.h"
#include "StringConstants.h"
#include "GUI/ImGuiService.h"

#include <algorithm>
#include <sstream>

Platform::WindowType Application::window = nullptr;

void Application::initialize_()
{
	Timer timer;
	if (mInitialized)
		return;

	if (window == nullptr)
	{
		Utils::ShowMessageBox("Window Not Initialized", "Error");
		exit(EXIT_ERROR);
	}

	Logger::Initialize();
	Input::Initialize(window);
	TextureCache::Initialize();
	ShaderPath::LoadStringPath();

   	mScene.Initialize();
	mScene.SetSize(mWidth, mHeight);

	mRenderer = std::make_unique<Renderer>(mWidth, mHeight);
	mRenderer->SetScene(&mScene);

	EventDispatcher::Subscribe(EventType::WindowResize, BIND_EVENT_FN(Application::windowResizeEvent));
	mInitialized = true;
	Logger::Debug("Intialized Application: (" + std::to_string(timer.elapsedSeconds()) + "s)");

	mGuiService = ui::ImGuiService::GetInstance();
	mGuiService->Init(window, mDevice);
}

void Application::Run()
{
	// New Profiler frame
	Profiler::BeginFrame();
	RangeId totalTime = Profiler::StartRangeCPU("Total CPU Time");

	Timer timer;
	if (!mInitialized)
	{
		Initialize();
		mInitialized = true;
	}

	mDeltaTime = float(std::max(0.0, mTimer.elapsed())) * 0.001f;
	mTimer.record();
	const float dt = mDeltaTime;
	mElapsedTime += dt;

	update_(dt);

	render_();

	mWindowTitle << "CPU Time: " << timer.elapsedMilliseconds() << "ms ";
	mWindowTitle << "FPS: " << 1.0f / mDeltaTime << " FrameTime: " << mDeltaTime * 1000.0f << "ms ";
	Platform::SetWindowTitle(window, mWindowTitle.str().c_str());
	mWindowTitle.str(std::string());
	
	Profiler::EndRangeCPU(totalTime);
	Profiler::EndFrame();
}

void Application::update_(float dt)
{
	PreUpdate(dt);
	mScene.Update(dt);
	mRenderer->Update(dt);
	Input::Update(window);
	PostUpdate(dt);
}

void Application::render_()
{
	if (!mDevice->IsSwapchainReady())
		return;

	RangeId cpuRenderTime = Profiler::StartRangeCPU("RenderTime CPU");

	// New GPU Frame
	mDevice->BeginFrame();

	gfx::CommandList commandList = mDevice->BeginCommandList();
	Profiler::BeginFrameGPU(&commandList);
	RangeId gpuRenderTime = Profiler::StartRangeGPU(&commandList, "RenderTime GPU");

	mDevice->PrepareSwapchain(&commandList);
	mRenderer->Render(&commandList);
	mDevice->PrepareSwapchainForPresent(&commandList);
	Profiler::EndRangeGPU(&commandList, gpuRenderTime);
	mDevice->Present(&commandList);
	Profiler::EndRangeCPU(cpuRenderTime);

}

void Application::SetWindow(Platform::WindowType window, bool fullscreen)
{
	this->window = window;

#if defined(USE_VULKAN)
	gfx::ValidationMode validationMode = gfx::ValidationMode::Enabled;
#ifndef ENABLE_VALIDATIONS
	validationMode = gfx::ValidationMode::Disabled;
#endif
	mDevice = std::make_unique<gfx::VulkanGraphicsDevice>(window, validationMode);
#else
#error Unsupported Graphics API
#endif
	gfx::GetDevice() = mDevice.get();

	// Create Default RenderPass
	WindowProperties props = {};
	Platform::GetWindowProperties(window, &props);
	mWidth = props.width;
	mHeight = props.height;
	
	// Create Swapchain
	mDevice->CreateSwapchain(window);
}

Application::~Application()
{
	// Wait for rendering to finish
	ui::ImGuiService::GetInstance()->Shutdown();
	TextureCache::Shutdown();
	mScene.Shutdown();
	mRenderer->Shutdown();
	mDevice->Shutdown();
}

bool Application::windowResizeEvent(const Event& evt)
{
	const WindowResizeEvent& resizeEvt = static_cast<const WindowResizeEvent&>(evt);
	mWidth = resizeEvt.getWidth();
	mHeight = resizeEvt.getHeight();
	mScene.SetSize(mWidth, mHeight);
	return true;
}

