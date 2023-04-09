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

	DebugDraw::Initialize(mSwapchainRP);

   	mScene.Initialize();
	TextureCache::Initialize(&mScene.mAsyncLoader);
	mScene.SetSize(mWidth, mHeight);

	mRenderer = std::make_unique<Renderer>();
	mRenderer->SetScene(&mScene);

	EventDispatcher::Subscribe(EventType::WindowResize, BIND_EVENT_FN(Application::windowResizeEvent));
	mInitialized = true;
	Logger::Debug("Intialized Application: (" + std::to_string(timer.elapsedSeconds()) + "s)");
}

void Application::Run()
{
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

	if (!mDevice->IsSwapchainReady(mSwapchainRP))
		return;

	mDevice->BeginFrame();
	gfx::CommandList commandList = mDevice->BeginCommandList();

	Profiler::BeginFrameGPU(&commandList);
	mDevice->PrepareSwapchain(&commandList);

	RangeId gpuRenderTime = Profiler::StartRangeGPU(&commandList, "RenderTime GPU");
	mRenderer->Render(&commandList);

	mDevice->BeginDebugMarker(&commandList, "SwapchainRP", 1.0f, 1.0f, 1.0f, 1.0f);
	RangeId swapchainRangeId = Profiler::StartRangeGPU(&commandList, "Swapchain Render");

	gfx::TextureHandle outputTexture = mRenderer->GetOutputTexture(OutputTextureType::HDROutput);
	gfx::ImageBarrierInfo shaderReadBarrier = { gfx::AccessFlag::ShaderReadWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, outputTexture };
	gfx::PipelineBarrierInfo shaderReadPipelineBarrier = { &shaderReadBarrier, 1, gfx::PipelineStage::ComputeShader, gfx::PipelineStage::FragmentShader};
	mDevice->PipelineBarrier(&commandList, &shaderReadPipelineBarrier);

	gfx::TextureHandle sceneDepthTexture = mRenderer->GetOutputTexture(OutputTextureType::HDRDepth);
	gfx::ImageBarrierInfo transferSrcBarrier = { gfx::AccessFlag::None, gfx::AccessFlag::TransferReadBit, gfx::ImageLayout::TransferSrcOptimal, sceneDepthTexture };
	gfx::PipelineBarrierInfo transferSrcPipelineBarrier = { &transferSrcBarrier, 1, gfx::PipelineStage::ComputeShader, gfx::PipelineStage::TransferBit };
	mDevice->PipelineBarrier(&commandList, &transferSrcPipelineBarrier);
	mDevice->CopyToSwapchain(&commandList, sceneDepthTexture, gfx::ImageLayout::DepthStencilAttachmentOptimal);

	mDevice->BeginRenderPass(&commandList, mSwapchainRP, gfx::INVALID_FRAMEBUFFER);
	gfx::DescriptorInfo descriptorInfo = { &outputTexture, 0, 0, gfx::DescriptorType::Image };
	mDevice->UpdateDescriptor(mSwapchainPipeline, &descriptorInfo, 1);
	mDevice->BindPipeline(&commandList, mSwapchainPipeline);
	mDevice->Draw(&commandList, 6, 0, 1);

	// Draw DebugData
	auto camera = mScene.GetCamera();
	glm::mat4 VP = camera->GetProjectionMatrix() * camera->GetViewMatrix();
	DebugDraw::Draw(&commandList, VP);

	RenderUI(&commandList);
	mDevice->EndRenderPass(&commandList);

	mDevice->EndDebugMarker(&commandList);
	Profiler::EndRangeGPU(&commandList, swapchainRangeId);
	
	mDevice->PrepareSwapchainForPresent(&commandList);
	Profiler::EndRangeGPU(&commandList, gpuRenderTime);

	mDevice->Present(&commandList);

	Profiler::EndRangeCPU(cpuRenderTime);
}

void Application::SetWindow(Platform::WindowType window, bool fullscreen)
{
	this->mWindow = window;

#if defined(USE_VULKAN)
	gfx::ValidationMode validationMode = gfx::ValidationMode::Enabled;
#ifdef NDEBUG
	validationMode = gfx::ValidationMode::Disabled;
#endif
	mDevice = std::make_unique<gfx::VulkanGraphicsDevice>(window, validationMode);
#else
#error Unsupported Graphics API
#endif
	gfx::GetDevice() = mDevice.get();

	// Create Default RenderPass
	WindowProperties props = {};
	Platform::GetWindowProperties(mWindow, &props);
	mWidth = props.width;
	mHeight = props.height;

	gfx::RenderPassDesc renderPassDesc = {};
	renderPassDesc.width = props.width;
	renderPassDesc.height = props.height;

	gfx::GPUTextureDesc colorAttachment;
	colorAttachment.bCreateSampler = false;
	colorAttachment.bindFlag = gfx::BindFlag::RenderTarget;
	colorAttachment.format = mSwapchainColorFormat;
	colorAttachment.width = props.width;
	colorAttachment.height = props.height;

	gfx::GPUTextureDesc depthAttachment;
	depthAttachment.bCreateSampler = false;
	depthAttachment.bindFlag = gfx::BindFlag::DepthStencil;
	depthAttachment.format = mSwapchainDepthFormat;
	depthAttachment.imageAspect = gfx::ImageAspect::Depth;
	depthAttachment.width = props.width;
	depthAttachment.height = props.height;
	gfx::Attachment attachments[] = { {0, colorAttachment}, {1, depthAttachment, true}};

	renderPassDesc.attachmentCount = (uint32_t)std::size(attachments);
	renderPassDesc.attachments = attachments;
	renderPassDesc.hasDepthAttachment = false;
	mSwapchainRP = mDevice->CreateRenderPass(&renderPassDesc);

	// Create Swapchain
	gfx::SwapchainDesc swapchainDesc = {};
	swapchainDesc.width = mWidth;
	swapchainDesc.height = mHeight;
	swapchainDesc.vsync = true;
	swapchainDesc.colorFormat = mSwapchainColorFormat;
	swapchainDesc.depthFormat = mSwapchainDepthFormat;
	swapchainDesc.fullscreen = false;
	swapchainDesc.enableDepth = true;
	swapchainDesc.renderPass = mSwapchainRP;
	swapchainDesc.clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	mDevice->CreateSwapchain(&swapchainDesc, window);

    // Create SwapchainPipeline
	uint32_t vertexLen = 0, fragmentLen = 0;
	char* vertexCode = Utils::ReadFile(StringConstants::COPY_VERT_PATH, &vertexLen);
	char* fragmentCode = Utils::ReadFile(StringConstants::COPY_FRAG_PATH, &fragmentLen);
	assert(vertexCode != nullptr);
	assert(fragmentCode != nullptr);

	gfx::PipelineDesc pipelineDesc = {};
	std::vector<gfx::ShaderDescription> shaders(2);
	shaders[0] = { vertexCode, vertexLen };
	shaders[1] = { fragmentCode, fragmentLen };
	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders.data();

	pipelineDesc.renderPass = mSwapchainRP;
	pipelineDesc.rasterizationState.enableDepthTest = false;
	pipelineDesc.rasterizationState.enableDepthWrite = false;
	pipelineDesc.rasterizationState.cullMode = gfx::CullMode::None;
	gfx::BlendState blendState = {};
	pipelineDesc.blendState = &blendState;
	pipelineDesc.blendStateCount = 1;

	mSwapchainPipeline = mDevice->CreateGraphicsPipeline(&pipelineDesc);
	delete[] vertexCode;
	delete[] fragmentCode;
}

Application::~Application()
{
	// Wait for rendering to finish
	mDevice->WaitForGPU();
	mDevice->Destroy(mSwapchainRP);
	mDevice->Destroy(mSwapchainPipeline);

	TextureCache::Shutdown();
	DebugDraw::Shutdown();
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

