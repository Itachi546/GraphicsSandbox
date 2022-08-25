#include "Renderer.h"
#include "Utils.h"
#include "Scene.h"
#include "Input.h"
#include "Logger.h"

#include "FX/Bloom.h"

#include <vector>
#include <algorithm>

Renderer::Renderer() : mDevice(gfx::GetDevice())
{
	initializeBuffers();
	mDevice->CreateSemaphore(&mAcquireSemaphore);
	mDevice->CreateSemaphore(&mReleaseSemaphore);

	gfx::RenderPassDesc desc;
	desc.width = 1920;
	desc.height = 1080;
	gfx::Attachment attachments[3] = 
	{
		gfx::Attachment{0, mHDRColorFormat, gfx::ImageLayout::ColorAttachmentOptimal},
		gfx::Attachment{1, mHDRColorFormat, gfx::ImageLayout::ColorAttachmentOptimal},
		gfx::Attachment{ 2, mHDRDepthFormat, gfx::ImageLayout::DepthStencilAttachmentOptimal },
	};

	desc.attachmentCount = static_cast<uint32_t>(std::size(attachments));
	desc.attachments = attachments;
	desc.hasDepthAttachment = true;
	mHdrRenderPass = std::make_shared<gfx::RenderPass>();
	mDevice->CreateRenderPass(&desc, mHdrRenderPass.get());

	mHdrFramebuffer = std::make_shared<gfx::Framebuffer>();
	mDevice->CreateFramebuffer(mHdrRenderPass.get(), mHdrFramebuffer.get());

	mTrianglePipeline = loadHDRPipeline("Assets/SPIRV/main.vert.spv", "Assets/SPIRV/main.frag.spv");
	mCubemapPipeline = loadHDRPipeline("Assets/SPIRV/cubemap.vert.spv", "Assets/SPIRV/cubemap.frag.spv", gfx::CullMode::None);

	mBloomFX = std::make_shared<fx::Bloom>(mDevice, desc.width, desc.height, mHDRColorFormat);
}

// TODO: temp width and height variable
void Renderer::Update(float dt)
{
	Camera* camera = mScene->GetCamera();

	glm::mat4 P = camera->GetProjectionMatrix();
	mGlobalUniformData.P = P;

	glm::mat4 V = camera->GetViewMatrix();
	mGlobalUniformData.V = V;
	mGlobalUniformData.VP = mGlobalUniformData.P * V;
	mGlobalUniformData.cameraPosition = camera->GetPosition();
	mGlobalUniformData.dt += dt;

	auto compMgr = mScene->GetComponentManager();
	auto lightArrComponent = compMgr->GetComponentArray<LightComponent>();
	std::vector<LightComponent>& lights = lightArrComponent->components;
	std::vector<ecs::Entity>& entities = lightArrComponent->entities;

	for (int i = 0; i < lights.size(); ++i)
	{
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(entities[i]);
		glm::vec3 position = glm::vec3(0.0f);

		if (lights[i].type == LightType::Directional)
			position = normalize(glm::toMat4(transform->rotation) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
		else if (lights[i].type == LightType::Point)
			position = transform->position;
		else {
			assert(!"Undefined Light Type");
		}
		mGlobalUniformData.lights[i] = LightData{ position, lights[i].radius, lights[i].color * lights[i].intensity, (float)lights[i].type };
	}

	uint32_t nLight = static_cast<uint32_t>(lights.size());
	mGlobalUniformData.nLight = nLight;
	//uint32_t uniformDataSize = sizeof(glm::mat4) * 3 + sizeof(float) * 4 + sizeof(int) + nLight * sizeof(LightData);
	std::memcpy(mGlobalUniformBuffer->mappedDataPtr, &mGlobalUniformData, sizeof(GlobalUniformData));
}

void Renderer::Render()
{
	gfx::CommandList commandList = mDevice->BeginCommandList();

	gfx::ImageBarrierInfo barriers[] = {
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ColorAttachmentWrite, gfx::ImageLayout::ColorAttachmentOptimal, &mHdrFramebuffer->attachments[0]},
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ColorAttachmentWrite, gfx::ImageLayout::ColorAttachmentOptimal, &mHdrFramebuffer->attachments[1]},
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::DepthStencilWrite, gfx::ImageLayout::DepthAttachmentOptimal, &mHdrFramebuffer->attachments[2]},
	};

	gfx::PipelineBarrierInfo colorAttachmentBarrier = { barriers, 2, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::ColorAttachmentOutput};
	gfx::PipelineBarrierInfo depthAttachmentBarrier = { &barriers[2], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::EarlyFramentTest};
	mDevice->PipelineBarrier(&commandList, &colorAttachmentBarrier);
	mDevice->PipelineBarrier(&commandList, &depthAttachmentBarrier);

	mDevice->PrepareSwapchain(&commandList, &mAcquireSemaphore);
	mDevice->BeginRenderPass(&commandList, mHdrRenderPass.get(), mHdrFramebuffer.get());

	/*
	* TODO: Currently PerObjectData is extracted from the
	* DrawData by iterating through it. In future maybe
	* we can do culling in the compute shader and set
	* PerObjectBuffer from compute shader.
	*/
	std::vector<DrawData> drawDatas;
	mScene->GenerateDrawData(drawDatas);

	auto& envMap = mScene->GetEnvironmentMap();

	gfx::GpuMemoryAllocator* allocator = gfx::GpuMemoryAllocator::GetInstance();

	mDevice->BeginDebugMarker(&commandList, "Draw Objects", 1.0f, 1.0f, 1.0f, 1.0f);
	if (drawDatas.size() > 0)
	{
		//DrawBatch(&commandList, drawDatas, allocator);

		std::array<std::vector<DrawData>, 64> sortedDrawDatas;
		for (auto& drawData : drawDatas)
		{
			uint32_t vbIndex = drawData.vertexBuffer.index;
			sortedDrawDatas[vbIndex].push_back(drawData);
		}

		uint32_t bufferOffset = 0;
		for (uint32_t i = 0; i < sortedDrawDatas.size(); ++i)
		{
			if (sortedDrawDatas[i].size() == 0)
				continue;

			DrawBatch(&commandList, sortedDrawDatas[i], bufferOffset, allocator);
			bufferOffset += (uint32_t)sortedDrawDatas[i].size();
		}
	}
	mDevice->EndDebugMarker(&commandList);


	gfx::GPUTexture* cubemap = envMap->GetCubemap().get();
	static int cubemapMode = 0;
	if (Input::Press(Input::Key::KEY_3))
		cubemapMode = 0;
	else if (Input::Press(Input::Key::KEY_4))
		cubemapMode = 1;
	else if (Input::Press(Input::Key::KEY_5))
		cubemapMode = 2;
	
	mDevice->BeginDebugMarker(&commandList, "Draw Cubemap", 1.0f, 1.0f, 1.0f, 1.0f);
	if(cubemapMode == 0)
		DrawCubemap(&commandList, envMap->GetCubemap().get());
	else if(cubemapMode == 1)
		DrawCubemap(&commandList, envMap->GetIrradianceMap().get());
	else 
		DrawCubemap(&commandList, envMap->GetPrefilterMap().get());

	mDevice->EndDebugMarker(&commandList);

	mDevice->EndRenderPass(&commandList);

	// Bloom Pass
	gfx::GPUTexture* hdrTexture = &mHdrFramebuffer->attachments[1];
	mBloomFX->Generate(&commandList, hdrTexture);

#if 0
	gfx::GPUTexture* outputTexture = mBloomFX->GetTexture();
	gfx::ImageBarrierInfo transferSrcBarrier = { gfx::AccessFlag::ShaderReadWrite, gfx::AccessFlag::TransferReadBit, gfx::ImageLayout::TransferSrcOptimal, outputTexture };
	gfx::PipelineBarrierInfo transferSrcPipelineBarrier = { &transferSrcBarrier, 1, gfx::PipelineStage::ComputeShader, gfx::PipelineStage::TransferBit };
#else 
	gfx::GPUTexture* outputTexture = &mHdrFramebuffer->attachments[0];
	gfx::ImageBarrierInfo transferSrcBarrier = { gfx::AccessFlag::ColorAttachmentWrite, gfx::AccessFlag::TransferReadBit, gfx::ImageLayout::TransferSrcOptimal, outputTexture };
	gfx::PipelineBarrierInfo transferSrcPipelineBarrier = { &transferSrcBarrier, 1, gfx::PipelineStage::ColorAttachmentOutput, gfx::PipelineStage::TransferBit };
#endif
	mDevice->PipelineBarrier(&commandList, &transferSrcPipelineBarrier);

	// Copy the output to swapchain
	mDevice->BeginDebugMarker(&commandList, "Copy To Swapchain", 1.0f, 1.0f, 1.0f, 1.0f);
	mDevice->CopyToSwapchain(&commandList, outputTexture, 0, 0);
	mDevice->EndDebugMarker(&commandList);

	mDevice->SubmitCommandList(&commandList, &mReleaseSemaphore);
	mDevice->Present(&mReleaseSemaphore);
	mDevice->WaitForGPU();
}

std::shared_ptr<gfx::Pipeline> Renderer::loadHDRPipeline(const char* vsPath, const char* fsPath, gfx::CullMode cullMode)
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

	pipelineDesc.renderPass = mHdrRenderPass.get();
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.cullMode = cullMode;
	pipelineDesc.rasterizationState.frontFace = gfx::FrontFace::Anticlockwise;

	gfx::BlendState blendState[2] = {};
	pipelineDesc.blendState = blendState;
	pipelineDesc.blendStateCount = static_cast<uint32_t>(std::size(blendState));

	auto pipeline = std::make_shared<gfx::Pipeline>();
	mDevice->CreateGraphicsPipeline(&pipelineDesc, pipeline.get());
	delete[] vertexCode;
	delete[] fragmentCode;
	return pipeline;
}

void Renderer::initializeBuffers()
{
	// Global Uniform Buffer
	gfx::GPUBufferDesc uniformBufferDesc = {};
	uniformBufferDesc.bindFlag = gfx::BindFlag::ConstantBuffer;
	uniformBufferDesc.usage = gfx::Usage::Upload;
	uniformBufferDesc.size = sizeof(GlobalUniformData);
	mGlobalUniformBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&uniformBufferDesc, mGlobalUniformBuffer.get());

	// Per Object Data Buffer
	gfx::GPUBufferDesc bufferDesc = {};
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = sizeof(PerObjectData) * kMaxEntity;
	mPerObjectDataBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&bufferDesc, mPerObjectDataBuffer.get());

	// Transform Buffer
	bufferDesc.size = kMaxEntity * sizeof(glm::mat4);
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	mTransformBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&bufferDesc, mTransformBuffer.get());

	// Material Component Buffer
	bufferDesc.size = kMaxEntity * sizeof(MaterialComponent);
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	mMaterialBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&bufferDesc, mMaterialBuffer.get());

	// Draw Indirect Buffer
	bufferDesc.size = kMaxEntity * sizeof(gfx::DrawIndirectCommand);
	bufferDesc.bindFlag = gfx::BindFlag::IndirectBuffer;
	mDrawIndirectBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&bufferDesc, mDrawIndirectBuffer.get());
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

void Renderer::DrawBatch(gfx::CommandList* commandList, std::vector<DrawData>& drawDatas, uint32_t lastOffset, gfx::GpuMemoryAllocator* allocator)
{
	// Copy PerObjectDrawData to the buffer
	PerObjectData* objectDataPtr = static_cast<PerObjectData*>(mPerObjectDataBuffer->mappedDataPtr) + lastOffset;
	glm::mat4* transformPtr = static_cast<glm::mat4*>(mTransformBuffer->mappedDataPtr) + lastOffset;
	MaterialComponent* materialCompPtr = static_cast<MaterialComponent*>(mMaterialBuffer->mappedDataPtr) + lastOffset;
	gfx::DrawIndirectCommand* drawCommandPtr = static_cast<gfx::DrawIndirectCommand*>(mDrawIndirectBuffer->mappedDataPtr) + lastOffset;

	for (uint32_t i = 0; i < drawDatas.size(); ++i)
	{
		transformPtr[i] = drawDatas[i].worldTransform;
		objectDataPtr[i].transformIndex = i;
		objectDataPtr[i].materialIndex = i;

		MaterialComponent* mat = drawDatas[i].material;
		std::memcpy(materialCompPtr + i, mat, sizeof(MaterialComponent));

		drawCommandPtr[i].firstIndex = drawDatas[i].indexBuffer.offset / sizeof(uint32_t);
		drawCommandPtr[i].indexCount = drawDatas[i].indexCount;
		drawCommandPtr[i].instanceCount = 1;
		drawCommandPtr[i].vertexOffset = drawDatas[i].vertexBuffer.offset / sizeof(Vertex);
	}

	gfx::BufferView& vbView = drawDatas[0].vertexBuffer;
	gfx::BufferView& ibView = drawDatas[0].indexBuffer;

	auto vb = allocator->GetBuffer(vbView.index);
	auto ib = allocator->GetBuffer(ibView.index);

	auto& envMap = mScene->GetEnvironmentMap();

	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[8] = {};

	descriptorInfos[0] = { mGlobalUniformBuffer.get(), 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };

	descriptorInfos[1] = { vb, 0, vb->desc.size,gfx::DescriptorType::StorageBuffer };

	descriptorInfos[2] = { mPerObjectDataBuffer.get(), (uint32_t)(lastOffset * sizeof(PerObjectData)), (uint32_t)(drawDatas.size() * sizeof(PerObjectData)), gfx::DescriptorType::StorageBuffer};

	descriptorInfos[3] = { mTransformBuffer.get(), (uint32_t)(lastOffset * sizeof(glm::mat4)), (uint32_t)(drawDatas.size() * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer};

	descriptorInfos[4] = { mMaterialBuffer.get(), (uint32_t)(lastOffset * sizeof(MaterialComponent)), (uint32_t)(drawDatas.size() * sizeof(MaterialComponent)), gfx::DescriptorType::StorageBuffer};

	descriptorInfos[5] = { envMap->GetIrradianceMap().get(), 0, 0, gfx::DescriptorType::Image };

	descriptorInfos[6] = { envMap->GetPrefilterMap().get(), 0, 0, gfx::DescriptorType::Image };

	descriptorInfos[7] = { envMap->GetBRDFLUT().get(), 0, 0, gfx::DescriptorType::Image };
	mDevice->UpdateDescriptor(mTrianglePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(commandList, mTrianglePipeline.get());

	mDevice->BindIndexBuffer(commandList, ib);
	mDevice->DrawIndexedIndirect(commandList, mDrawIndirectBuffer.get(), lastOffset * sizeof(gfx::DrawIndirectCommand), (uint32_t)drawDatas.size(), sizeof(gfx::DrawIndirectCommand));
}
