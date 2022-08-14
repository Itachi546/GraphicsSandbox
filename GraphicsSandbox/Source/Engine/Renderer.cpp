#include "Renderer.h"
#include "GraphicsDevice.h"
#include "Utils.h"
#include "Scene.h"

#include <vector>

Renderer::Renderer(std::shared_ptr<gfx::RenderPass> swapchainRP) : mSwapchainRP(swapchainRP)
{
	mDevice = gfx::GetDevice();
	
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

	mDevice->CreateSemaphore(&mAcquireSemaphore);
	mDevice->CreateSemaphore(&mReleaseSemaphore);

	mTrianglePipeline = loadPipelines("Assets/SPIRV/main.vert.spv", "Assets/SPIRV/main.frag.spv");
	mCubemapPipeline = loadPipelines("Assets/SPIRV/cubemap.vert.spv", "Assets/SPIRV/cubemap.frag.spv", gfx::CullMode::None);
}

// TODO: temp width and height variable
void Renderer::Update(float dt, int width, int height)
{
	Camera* camera = mScene->GetCamera();
	if (width > 0 || height > 0)
	{
		camera->SetAspect(width / float(height));
		glm::mat4 P = camera->GetProjectionMatrix();
		mGlobalUniformData.P = P;
	}

	glm::mat4 V = camera->GetViewMatrix();
	mGlobalUniformData.V = V;
	mGlobalUniformData.VP = mGlobalUniformData.P * V;
	mGlobalUniformData.cameraPosition = camera->GetPosition();
	mGlobalUniformData.dt += dt;

	std::memcpy(mGlobalUniformBuffer->mappedDataPtr, &mGlobalUniformData, sizeof(GlobalUniformData));
}

void Renderer::Render()
{
	gfx::CommandList commandList = mDevice->BeginCommandList();

	mDevice->PrepareSwapchain(&commandList, &mAcquireSemaphore);
	mDevice->BeginRenderPass(&commandList, mSwapchainRP.get());

	/*
	* TODO: Currently PerObjectData is extracted from the
	* DrawData by iterating through it. In future maybe
	* we can do culling in the compute shader and set
	* PerObjectBuffer from compute shader.
	*/
	std::vector<DrawData> drawDatas;
	mScene->GenerateDrawData(drawDatas);

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

	gfx::GpuMemoryAllocator* allocator = gfx::GpuMemoryAllocator::GetInstance();
	gfx::BufferView& vbView = drawDatas[0].vertexBuffer;
	gfx::BufferView& ibView = drawDatas[0].indexBuffer;

	auto vb = allocator->GetBuffer(vbView.index);
	auto ib = allocator->GetBuffer(ibView.index);

	auto& envMap = mScene->GetEnvironmentMap();
	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[4] = {};
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

	descriptorInfos[3].resource = envMap->GetCubemap().get();
	descriptorInfos[3].type = gfx::DescriptorType::Image;

	mDevice->UpdateDescriptor(mTrianglePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(&commandList, mTrianglePipeline.get());

	mDevice->BindIndexBuffer(&commandList, ib);
	mDevice->DrawIndexedIndirect(&commandList, mDrawIndirectBuffer.get(), 0, (uint32_t)drawDatas.size(), sizeof(gfx::DrawIndirectCommand));

	DrawCubemap(&commandList, envMap->GetCubemap().get());

	mDevice->EndRenderPass(&commandList);
	mDevice->SubmitCommandList(&commandList, &mReleaseSemaphore);
	mDevice->Present(&mReleaseSemaphore);
	mDevice->WaitForGPU();
}

std::shared_ptr<gfx::Pipeline> Renderer::loadPipelines(const char* vsPath, const char* fsPath, gfx::CullMode cullMode)
{
	// Create PBR Pipeline
	uint32_t vertexLen = 0, fragmentLen = 0;
	char* vertexCode = Utils::ReadFile(vsPath, &vertexLen);
	char* fragmentCode = Utils::ReadFile(fsPath, &fragmentLen);
	assert(vertexCode != nullptr);
	assert(fragmentCode != nullptr);

	gfx::PipelineDesc pipelineDesc = {};
	std::vector<gfx::ShaderDescription> shaders(2);
	shaders[0] = { vertexCode, vertexLen };
	shaders[1] = { fragmentCode, fragmentLen };
	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders.data();

	// TODO: Remove the swapchainRP, copy the final data to swapchain
	pipelineDesc.renderPass = mSwapchainRP.get();
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.cullMode = cullMode;
	pipelineDesc.rasterizationState.frontFace = gfx::FrontFace::Anticlockwise;

	auto pipeline = std::make_shared<gfx::Pipeline>();
	mDevice->CreateGraphicsPipeline(&pipelineDesc, pipeline.get());
	delete[] vertexCode;
	delete[] fragmentCode;
	return pipeline;
}

void Renderer::DrawCubemap(gfx::CommandList* commandList, gfx::GPUTexture* cubemap)
{
	MeshDataComponent* meshData = mScene->mComponentManager->GetComponent<MeshDataComponent>(mScene->mCubeEntity);

	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[3] = {};
	descriptorInfos[0].resource = mGlobalUniformBuffer.get();
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].size = sizeof(GlobalUniformData);
	descriptorInfos[0].type = gfx::DescriptorType::UniformBuffer;

	gfx::GpuMemoryAllocator* allocator = gfx::GpuMemoryAllocator::GetInstance();
	auto vbView = meshData->vertexBuffer;
	auto ibView = meshData->indexBuffer;

	descriptorInfos[1].resource = allocator->GetBuffer(vbView.index);
	descriptorInfos[1].offset = vbView.offset;
	descriptorInfos[1].size = vbView.size;
	descriptorInfos[1].type = gfx::DescriptorType::StorageBuffer;

	descriptorInfos[2].resource = cubemap;
	descriptorInfos[2].offset = 0;
	descriptorInfos[2].type = gfx::DescriptorType::Image;

	mDevice->UpdateDescriptor(mCubemapPipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(commandList, mCubemapPipeline.get());
	mDevice->BindIndexBuffer(commandList, allocator->GetBuffer(ibView.index));
	mDevice->DrawTriangleIndexed(commandList, 36, 1, 0);
}
