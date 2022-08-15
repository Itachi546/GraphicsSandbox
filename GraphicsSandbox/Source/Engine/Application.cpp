#include "Application.h"
#include "Input.h"
#include "Logger.h"
#include "Utils.h"
#include "VulkanGraphicsDevice.h"
#include "Scene.h"

#include <algorithm>
#include <sstream>

void Application::initialize_()
{
	Timer timer;
	if (mInitialized)
		return;

	if (mWindow == nullptr)
	{
		Utils::ShowMessageBox("Window Not Initialized", "Error");
		exit(EXIT_ERROR);
	}

	Logger::Initialize();
	Input::Initialize(mWindow);
   	mScene.Initialize();

	mRenderer = std::make_unique<Renderer>(mSwapchainRP);
	mRenderer->SetScene(&mScene);

	EventDispatcher::Subscribe(EventType::WindowResize, BIND_EVENT_FN(Application::windowResizeEvent));
	mInitialized = true;
	Logger::Debug("Intialized Application: (" + std::to_string(timer.elapsedSeconds()) + "s)");
}

void Application::Run()
{
	Timer timer;

	if (!mInitialized)
	{
		Initialize();
		mInitialized = true;
	}

	mDeltaTime = float(std::max(0.0, mTimer.elapsed()));
	mTimer.record();
	const float dt = mLockFrameRate ? (1.0f / mTargetFrameRate) : mDeltaTime;
	mElapsedTime += dt;

	update_(dt);

	render_();

	mWindowTitle << "CPU Time: " << timer.elapsedMilliseconds() << "ms ";
	mWindowTitle << "FPS: " << 1.0f / dt << " FrameTime: " << dt * 1000.0f << "ms ";
	Platform::SetWindowTitle(mWindow, mWindowTitle.str().c_str());

	mWindowTitle.str(std::string());
}

void Application::update_(float dt)
{
	PreUpdate(dt);
	mScene.Update(dt);
	mRenderer->Update(dt, mWidth, mHeight);
	Input::Update(mWindow);
	PostUpdate(dt);
}

void Application::render_()
{

	if (!mDevice->IsSwapchainReady(mSwapchainRP.get()))
		return;

	mRenderer->Render();
}

void Application::SetWindow(Platform::WindowType window, bool fullscreen)
{
	this->mWindow = window;

#if defined(USE_VULKAN)
	gfx::ValidationMode validationMode = gfx::ValidationMode::Enabled;
	mDevice = std::make_unique<gfx::VulkanGraphicsDevice>(window, validationMode);
#else
#error Unsupported Graphics API
#endif
	gfx::GetDevice() = mDevice.get();

	// Create Default RenderPass
	mSwapchainRP = std::make_shared<gfx::RenderPass>();
	WindowProperties props = {};
	Platform::GetWindowProperties(mWindow, &props);
	mWidth = props.width;
	mHeight = props.height;

	gfx::RenderPassDesc renderPassDesc = {};
	renderPassDesc.width = props.width;
	renderPassDesc.height = props.height;

	gfx::Attachment attachments[2] = {
		gfx::Attachment{0, mSwapchainColorFormat, gfx::ImageLayout::ColorAttachmentOptimal},
		gfx::Attachment{ 1, mSwapchainDepthFormat, gfx::ImageLayout::DepthStencilAttachmentOptimal },
	};

	renderPassDesc.attachmentCount = static_cast<uint32_t>(bSwapchainDepthSupport ? std::size(attachments) : std::size(attachments) - 1);
	renderPassDesc.attachments = attachments;
	renderPassDesc.hasDepthAttachment = bSwapchainDepthSupport;
	mDevice->CreateRenderPass(&renderPassDesc, mSwapchainRP.get());

	// Create Swapchain
	gfx::SwapchainDesc swapchainDesc = {};
	swapchainDesc.width = mWidth;
	swapchainDesc.height = mHeight;
	swapchainDesc.vsync = true;
	swapchainDesc.colorFormat = mSwapchainColorFormat;
	swapchainDesc.depthFormat = mSwapchainDepthFormat;	
	swapchainDesc.fullscreen = false;
	swapchainDesc.enableDepth = bSwapchainDepthSupport;
	swapchainDesc.renderPass = mSwapchainRP.get();
	swapchainDesc.clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

	mDevice->CreateSwapchain(&swapchainDesc, window);
}

Application::~Application()
{
}

bool Application::windowResizeEvent(const Event& evt)
{
	const WindowResizeEvent& resizeEvt = static_cast<const WindowResizeEvent&>(evt);
	mWidth = resizeEvt.getWidth();
	mHeight = resizeEvt.getHeight();
	return true;
}

