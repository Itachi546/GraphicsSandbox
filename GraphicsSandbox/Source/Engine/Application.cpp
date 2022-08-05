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

	EventDispatcher::Subscribe(EventType::WindowResize, BIND_EVENT_FN(Application::windowResizeEvent));
	mInitialized = true;

	// Create Pipeline
	mTrianglePipeline = std::make_shared<gfx::Pipeline>();
	uint32_t vertexLen = 0, fragmentLen = 0;
	char* vertexCode =   Utils::ReadFile("SPIRV/main.vert.spv", &vertexLen);
	char* fragmentCode = Utils::ReadFile("SPIRV/main.frag.spv", &fragmentLen);
	assert(vertexCode != nullptr);
	assert(fragmentCode != nullptr);

	gfx::PipelineDesc pipelineDesc = {};
	std::vector<gfx::ShaderDescription> shaders(2);
	shaders[0] = { vertexCode, vertexLen };
	shaders[1] = { fragmentCode, fragmentLen };
	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders.data();
	pipelineDesc.renderPass = mSwapchainRP.get();
	pipelineDesc.rasterizationState.enableDepthTest  = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.cullMode = gfx::CullMode::Back;
	pipelineDesc.rasterizationState.frontFace = gfx::FrontFace::Anticlockwise;
	mDevice->CreateGraphicsPipeline(&pipelineDesc, mTrianglePipeline.get());

	delete[] vertexCode;
	delete[] fragmentCode;

	// Global Uniform Buffer
	gfx::GPUBufferDesc uniformBufferDesc = {};
	uniformBufferDesc.bindFlag = gfx::BindFlag::ConstantBuffer;
	uniformBufferDesc.usage = gfx::Usage::Upload;
	uniformBufferDesc.size = sizeof(GlobalUniformData);
	mGlobalUniformBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&uniformBufferDesc, mGlobalUniformBuffer.get());

	// Per Object Transform Buffer
	gfx::GPUBufferDesc bufferDesc = {};
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = sizeof(PerObjectData) * kMaxEntity;
	mPerObjectDataBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&bufferDesc, mPerObjectDataBuffer.get());

	bufferDesc.size = kMaxEntity * sizeof(gfx::DrawIndirectCommand);
	bufferDesc.bindFlag = gfx::BindFlag::IndirectBuffer;
	mDrawIndirectBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&bufferDesc, mDrawIndirectBuffer.get());

	// Create Cube 
	mEntity = mScene.CreateCube("plane1");
	ecs::Entity plane = mScene.CreatePlane("cube1");

	TransformComponent* transform = mScene.GetComponentManager()->GetComponent<TransformComponent>(plane);
	transform->scale = glm::vec3(5.0f);
	transform->position = glm::vec3(0.0f, -1.0f, 0.0f);
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

	Update(dt);

	Render();

	mWindowTitle << "CPU Time: " << timer.elapsedMilliseconds() << "ms ";
	mWindowTitle << "FPS: " << 1.0f / dt << " FrameTime: " << dt * 1000.0f << "ms ";
	Platform::SetWindowTitle(mWindow, mWindowTitle.str().c_str());

	mWindowTitle.str(std::string());
}

void Application::Update(float dt)
{
	mScene.Update(dt);

	if (mWidth > 0.0f || mHeight > 0.0f)
	{
		glm::mat4 P = glm::perspective(glm::radians(70.0f), mWidth / float(mHeight), 0.3f, 1000.0f);
		mGlobalUniformData.P = P;

		glm::mat4 V = glm::lookAt(glm::vec3(0.0f, 3.0f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		mGlobalUniformData.V = V;
		mGlobalUniformData.VP = P * V;
	}

	std::memcpy(mGlobalUniformBuffer->mappedDataPtr, &mGlobalUniformData, sizeof(GlobalUniformData));

	Input::Update(mWindow);

	TransformComponent* transform = mScene.GetComponentManager()->GetComponent<TransformComponent>(mEntity);
	static float angle = 0.0f;
	transform->rotation = glm::fquat{ glm::vec3(0.0f, angle, 0.0f) };
	transform->SetDirty(true);
	angle += dt * 0.5f;
}

void Application::Render()
{

	if (!mDevice->IsSwapchainReady(mSwapchainRP.get()))
		return;

	gfx::CommandList commandList = mDevice->BeginCommandList();
	mDevice->BeginRenderPass(&commandList, mSwapchainRP.get());
	
	/*
	* TODO: Currently PerObjectData is extracted from the
	* DrawData by iterating through it. In future maybe 
	* we can do culling in the compute shader and set 
	* PerObjectBuffer from compute shader.
	*/
	std::vector<DrawData> drawDatas;
	mScene.GenerateDrawData(drawDatas);

	// Copy PerObjectDrawData to the buffer
	PerObjectData* objectDataPtr = static_cast<PerObjectData*>(mPerObjectDataBuffer->mappedDataPtr);
	gfx::DrawIndirectCommand* drawCommandPtr = static_cast<gfx::DrawIndirectCommand*>(mDrawIndirectBuffer->mappedDataPtr);

	for (uint32_t i = 0; i < drawDatas.size(); ++i)
	{
		objectDataPtr[i].transform = drawDatas[i].worldTransform;

		drawCommandPtr[i].firstIndex = drawDatas[i].indexBuffer.offset / sizeof(uint32_t);
		drawCommandPtr[i].indexCount = drawDatas[i].indexCount;
		drawCommandPtr[i].instanceCount = 1;
		drawCommandPtr[i].vertexOffset = drawDatas[i].vertexBuffer.offset / sizeof(Vertex);
	}
	//std::memcpy(mPerObjectDataBuffer->mappedDataPtr, perObjectData.data(), sizeof(PerObjectData) * perObjectData.size());
	//std::memcpy(mDrawIndirectBuffer->mappedDataPtr, drawIndirectCommands.data(), drawIndirectCommands.size() * sizeof(gfx::DrawIndirectCommand));

	gfx::GpuMemoryAllocator* allocator = gfx::GpuMemoryAllocator::GetInstance();
	gfx::BufferView& vbView = drawDatas[0].vertexBuffer;
	gfx::BufferView& ibView = drawDatas[0].indexBuffer;

	auto vb = allocator->GetBuffer(vbView.index);
	auto ib = allocator->GetBuffer(ibView.index);
 
	gfx::DescriptorInfo descriptorInfos[3] = {};
	descriptorInfos[0].resource = mGlobalUniformBuffer.get();
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].size = sizeof(GlobalUniformData);
	descriptorInfos[0].type = gfx::DescriptorType::UniformBuffer;

	descriptorInfos[1].resource = vb;
	descriptorInfos[1].offset = 0;
	descriptorInfos[1].size = vb->desc.size;
	descriptorInfos[1].type = gfx::DescriptorType::StorageBuffer;

	gfx::GPUBuffer* perObjectBuffer = mPerObjectDataBuffer.get();
	descriptorInfos[2].resource = perObjectBuffer;
	descriptorInfos[2].offset = 0;
	descriptorInfos[2].size = (uint32_t)perObjectBuffer->desc.size;
	descriptorInfos[2].type = gfx::DescriptorType::StorageBuffer;

	mDevice->UpdateDescriptor(mTrianglePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(&commandList, mTrianglePipeline.get());

	mDevice->BindIndexBuffer(&commandList, ib);
	mDevice->DrawIndexedIndirect(&commandList, mDrawIndirectBuffer.get(), 0, (uint32_t)drawDatas.size(), sizeof(gfx::DrawIndirectCommand));
	
	mDevice->EndRenderPass(&commandList);
	mDevice->SubmitCommandList(&commandList);
	mDevice->Present();
	mDevice->WaitForGPU();
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

