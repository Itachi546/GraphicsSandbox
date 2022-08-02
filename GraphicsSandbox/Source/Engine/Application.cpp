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
	pipelineDesc.rasterizationState.depthTestFunc = gfx::CompareOp::Greater;
	pipelineDesc.rasterizationState.cullMode = gfx::CullMode::Back;
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

	// Create Cube 
	mEntity = mScene.CreateCube("cube");
	mEntityTransform = mScene.GetComponentManager()->GetComponent<TransformComponent>(mEntity);
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

	glm::mat4 P = glm::perspective(glm::radians(70.0f), mWidth / float(mHeight), 0.3f, 1000.0f);
	glm::mat4 V = glm::lookAt(glm::vec3(0.0f, 3.0f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	mGlobalUniformData.P = P;
	mGlobalUniformData.V = V;
	mGlobalUniformData.VP = P * V;
	std::memcpy(mGlobalUniformBuffer->mappedDataPtr, &mGlobalUniformData, sizeof(GlobalUniformData));

	Input::Update(mWindow);

	static float angle = 0.0f;
	mEntityTransform->rotation = glm::fquat{glm::vec3(angle)};
	mEntityTransform->SetDirty(true);

	angle += 0.005f;
}

void Application::Render()
{

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
	std::vector<PerObjectData> perObjectData;
	std::for_each(drawDatas.begin(), drawDatas.end(), [&perObjectData](DrawData& drawData) {
		perObjectData.emplace_back(std::move(drawData.worldTransform));
		});
	std::memcpy(mPerObjectDataBuffer->mappedDataPtr, perObjectData.data(), sizeof(PerObjectData) * perObjectData.size());

	for (auto& drawData : drawDatas)
	{
		gfx::DescriptorInfo descriptorInfos[3] = {};
		descriptorInfos[0].resource = mGlobalUniformBuffer.get();
		descriptorInfos[0].offset = 0;
		descriptorInfos[0].size = sizeof(GlobalUniformData);
		descriptorInfos[0].type = gfx::DescriptorType::UniformBuffer;

		gfx::GPUBuffer* vb = drawData.vertexBuffer;
		descriptorInfos[1].resource = vb;
		descriptorInfos[1].offset = 0;
		descriptorInfos[1].size = (uint32_t)vb->desc.size;
		descriptorInfos[1].type = gfx::DescriptorType::StorageBuffer;

		gfx::GPUBuffer* perObjectBuffer = mPerObjectDataBuffer.get();
		descriptorInfos[2].resource = perObjectBuffer;
		descriptorInfos[2].offset = 0;
		descriptorInfos[2].size = (uint32_t)perObjectBuffer->desc.size;
		descriptorInfos[2].type = gfx::DescriptorType::StorageBuffer;

		mDevice->UpdateDescriptor(mTrianglePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
		mDevice->BindPipeline(&commandList, mTrianglePipeline.get());

		mDevice->BindIndexBuffer(&commandList, drawData.indexBuffer);
		mDevice->DrawTriangleIndexed(&commandList, drawData.indexCount, 1, 0);
	}
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

