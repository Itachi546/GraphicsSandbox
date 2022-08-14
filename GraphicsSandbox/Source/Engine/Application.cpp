#include "Application.h"
#include "Input.h"
#include "Logger.h"
#include "Utils.h"
#include "VulkanGraphicsDevice.h"
#include "Scene.h"

#include <algorithm>
#include <sstream>

void Application::Initialize()
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

	
	// Create Cube 
	mEntity = mScene.CreateSphere("sphere1");
	ecs::Entity plane = mScene.CreatePlane("plane1");

	TransformComponent* transform = mScene.GetComponentManager()->GetComponent<TransformComponent>(plane);
	transform->scale = glm::vec3(5.0f);
	transform->position = glm::vec3(0.0f, -1.0f, 0.0f);

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

	Update(dt);

	Render();

	mWindowTitle << "CPU Time: " << timer.elapsedMilliseconds() << "ms ";
	mWindowTitle << "FPS: " << 1.0f / dt << " FrameTime: " << dt * 1000.0f << "ms ";
	Platform::SetWindowTitle(mWindow, mWindowTitle.str().c_str());

	mWindowTitle.str(std::string());
}

void Application::Update(float dt)
{
	Camera* camera = mScene.GetCamera();

	const float walkSpeed = 1.0f;

	if (Input::Down(Input::Key::KEY_ESCAPE))
		bRunning = false;

	if (Input::Down(Input::Key::KEY_W))
		camera->Walk(-dt * walkSpeed);
	else if (Input::Down(Input::Key::KEY_S))
		camera->Walk(dt * walkSpeed);
	if (Input::Down(Input::Key::KEY_A))
		camera->Strafe(-dt * walkSpeed);
	else if (Input::Down(Input::Key::KEY_D))
		camera->Strafe(dt * walkSpeed);
	if (Input::Down(Input::Key::KEY_1))
		camera->Lift(dt * walkSpeed);
	else if (Input::Down(Input::Key::KEY_2))
		camera->Lift(-dt * walkSpeed);

	if (Input::Down(Input::Key::MOUSE_BUTTON_LEFT))
	{
		auto[x, y] = Input::GetMouseState().delta;
		camera->Rotate(-y, x, dt);
	}

	mScene.Update(dt);
	mRenderer->Update(dt, mWidth, mHeight);
	Input::Update(mWindow);

	TransformComponent* transform = mScene.GetComponentManager()->GetComponent<TransformComponent>(mEntity);
	static float angle = 0.75f;
	transform->rotation = glm::fquat{ glm::vec3(0.0f, angle, 0.0f) };
	transform->SetDirty(true);
	/*
	angle += dt * 0.1f;
	*/
}

void Application::Render()
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
	swapchainDesc.vsync = false;
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

