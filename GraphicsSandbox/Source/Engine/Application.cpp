#include "Application.h"
#include "Input.h"
#include "Logger.h"
#include "Utils.h"
#include "VulkanGraphicsDevice.h"
#include "Scene.h"
#include "Profiler.h"

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
	mScene.SetSize(mWidth, mHeight);

	mRenderer = std::make_unique<Renderer>();
	mRenderer->SetScene(&mScene);

	EventDispatcher::Subscribe(EventType::WindowResize, BIND_EVENT_FN(Application::windowResizeEvent));
	mInitialized = true;
	Logger::Debug("Intialized Application: (" + std::to_string(timer.elapsedSeconds()) + "s)");
}

void Application::Run()
{
	Profiler::BeginFrame();

	RangeId totalTime = Profiler::StartRangeCPU("Total CPU Time");

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
	
	Profiler::EndRangeCPU(totalTime);
	Profiler::EndFrame();
}

void Application::update_(float dt)
{
	PreUpdate(dt);
	mScene.Update(dt);
	mRenderer->Update(dt);
	Input::Update(mWindow);
	PostUpdate(dt);
}

void Application::render_()
{
	RangeId cpuRenderTime = Profiler::StartRangeCPU("RenderTime CPU");

	if (!mDevice->IsSwapchainReady(mSwapchainRP.get()))
		return;

	gfx::CommandList commandList = mDevice->BeginCommandList();
	RangeId gpuRenderTime = Profiler::StartRangeGPU(&commandList, "RenderTime GPU");

	mDevice->PrepareSwapchain(&commandList, &mAcquireSemaphore);

	mRenderer->Render(&commandList);

	mDevice->BeginDebugMarker(&commandList, "SwapchainRP", 1.0f, 1.0f, 1.0f, 1.0f);
	gfx::GPUTexture* outputTexture = mRenderer->GetOutputTexture(OutputTextureType::HDROutput);
	gfx::ImageBarrierInfo transferSrcBarrier = { gfx::AccessFlag::ShaderReadWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, outputTexture };
	gfx::PipelineBarrierInfo transferSrcPipelineBarrier = { &transferSrcBarrier, 1, gfx::PipelineStage::ComputeShader, gfx::PipelineStage::FragmentShader};

	mDevice->PipelineBarrier(&commandList, &transferSrcPipelineBarrier);

	mDevice->BeginRenderPass(&commandList, mSwapchainRP.get(), nullptr);
	gfx::DescriptorInfo descriptorInfo = { outputTexture, 0, 0, gfx::DescriptorType::Image };
	mDevice->UpdateDescriptor(mSwapchainPipeline.get(), &descriptorInfo, 1);
	mDevice->BindPipeline(&commandList, mSwapchainPipeline.get());
	mDevice->DrawTriangle(&commandList, 6, 0, 1);

	RenderUI(&commandList);

	mDevice->EndRenderPass(&commandList);
	mDevice->EndDebugMarker(&commandList);

	/*
	// Copy the output to swapchain
	mDevice->BeginDebugMarker(&commandList, "Copy To Swapchain", 1.0f, 1.0f, 1.0f, 1.0f);
	mDevice->CopyToSwapchain(&commandList, outputTexture, 0, 0);
	mDevice->EndDebugMarker(&commandList);
    */
	mDevice->PrepareSwapchainForPresent(&commandList);
	Profiler::EndRangeGPU(&commandList, gpuRenderTime);

	mDevice->SubmitCommandList(&commandList, &mReleaseSemaphore);
	mDevice->Present(&mReleaseSemaphore);
	mDevice->WaitForGPU();

	Profiler::EndRangeCPU(cpuRenderTime);
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

	gfx::Attachment attachment{ 0, mSwapchainColorFormat, gfx::ImageLayout::ColorAttachmentOptimal };

	renderPassDesc.attachmentCount = 1;
	renderPassDesc.attachments = &attachment;
	renderPassDesc.hasDepthAttachment = false;
	mDevice->CreateRenderPass(&renderPassDesc, mSwapchainRP.get());

	// Create Swapchain
	gfx::SwapchainDesc swapchainDesc = {};
	swapchainDesc.width = mWidth;
	swapchainDesc.height = mHeight;
	swapchainDesc.vsync = false;
	swapchainDesc.colorFormat = mSwapchainColorFormat;
	swapchainDesc.fullscreen = false;
	swapchainDesc.enableDepth = false;
	swapchainDesc.renderPass = mSwapchainRP.get();
	swapchainDesc.clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	mDevice->CreateSwapchain(&swapchainDesc, window);

	mDevice->CreateSemaphore(&mAcquireSemaphore);
	mDevice->CreateSemaphore(&mReleaseSemaphore);

    // Create SwapchainPipeline
	uint32_t vertexLen = 0, fragmentLen = 0;
	char* vertexCode = Utils::ReadFile("Assets/SPIRV/copy.vert.spv", &vertexLen);
	char* fragmentCode = Utils::ReadFile("Assets/SPIRV/copy.frag.spv", &fragmentLen);
	assert(vertexCode != nullptr);
	assert(fragmentCode != nullptr);

	gfx::PipelineDesc pipelineDesc = {};
	std::vector<gfx::ShaderDescription> shaders(2);
	shaders[0] = { vertexCode, vertexLen };
	shaders[1] = { fragmentCode, fragmentLen };
	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders.data();

	pipelineDesc.renderPass = mSwapchainRP.get();
	pipelineDesc.rasterizationState.enableDepthTest = false;
	pipelineDesc.rasterizationState.cullMode = gfx::CullMode::None;
	gfx::BlendState blendState = {};
	pipelineDesc.blendState = &blendState;
	pipelineDesc.blendStateCount = 1;

	mSwapchainPipeline = std::make_shared<gfx::Pipeline>();
	mDevice->CreateGraphicsPipeline(&pipelineDesc, mSwapchainPipeline.get());
	delete[] vertexCode;
	delete[] fragmentCode;
}

Application::~Application()
{
}

bool Application::windowResizeEvent(const Event& evt)
{
	const WindowResizeEvent& resizeEvt = static_cast<const WindowResizeEvent&>(evt);
	mWidth = resizeEvt.getWidth();
	mHeight = resizeEvt.getHeight();
	mScene.SetSize(mWidth, mHeight);
	return true;
}

