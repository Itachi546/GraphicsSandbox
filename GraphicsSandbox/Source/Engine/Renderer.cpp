#include "Renderer.h"
#include "Utils.h"
#include "Scene.h"
#include "Input.h"
#include "Logger.h"
#include "Profiler.h"
#include "FX/Bloom.h"
#include "CascadedShadowMap.h"
#include "StringConstants.h"

#include "../Shared/MeshData.h"
#include <vector>
#include <algorithm>

#include "TextureCache.h"

Renderer::Renderer() : mDevice(gfx::GetDevice())
{
	initializeBuffers();

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

	mTrianglePipeline = loadHDRPipeline(StringConstants::MAIN_VERT_PATH, StringConstants::MAIN_FRAG_PATH);
	mCubemapPipeline = loadHDRPipeline(StringConstants::CUBEMAP_VERT_PATH, StringConstants::CUBEMAP_FRAG_PATH, gfx::CullMode::None);

	mBloomFX = std::make_shared<fx::Bloom>(mDevice, desc.width, desc.height, mHDRColorFormat);
	mShadowMap = std::make_shared<CascadedShadowMap>();

	mGlobalUniformData.enabledNormalMapping = true;
}

// TODO: temp width and height variable
void Renderer::Update(float dt)
{
	if (mUpdateBatches)
	{
		CreateBatch();
		mUpdateBatches = false;
	}
	Camera* camera = mScene->GetCamera();

	glm::mat4 P = camera->GetProjectionMatrix();
	mGlobalUniformData.P = P;

	glm::mat4 V = camera->GetViewMatrix();
	mGlobalUniformData.V = V;
	mGlobalUniformData.VP = mGlobalUniformData.P * V;
	mGlobalUniformData.cameraPosition = camera->GetPosition();
	mGlobalUniformData.dt += dt;
	mGlobalUniformData.bloomThreshold = mBloomThreshold;

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
			position = transform->worldMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
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

void Renderer::Render(gfx::CommandList* commandList)
{
	gfx::ImageBarrierInfo barriers[] = {
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ColorAttachmentWrite, gfx::ImageLayout::ColorAttachmentOptimal, &mHdrFramebuffer->attachments[0]},
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ColorAttachmentWrite, gfx::ImageLayout::ColorAttachmentOptimal, &mHdrFramebuffer->attachments[1]},
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::DepthStencilWrite, gfx::ImageLayout::DepthAttachmentOptimal, &mHdrFramebuffer->attachments[2]},
	};

	gfx::PipelineBarrierInfo colorAttachmentBarrier = { barriers, 2, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::ColorAttachmentOutput};
	gfx::PipelineBarrierInfo depthAttachmentBarrier = { &barriers[2], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::EarlyFramentTest};
	mDevice->PipelineBarrier(commandList, &colorAttachmentBarrier);
	mDevice->PipelineBarrier(commandList, &depthAttachmentBarrier);

	auto hdrPass = Profiler::StartRangeGPU(commandList, "HDR Pass");
	mDevice->BeginRenderPass(commandList, mHdrRenderPass.get(), mHdrFramebuffer.get());

	/*
	* TODO: Currently PerObjectData is extracted from the
	* DrawData by iterating through it. In future maybe
	* we can do culling in the compute shader and set
	* PerObjectBuffer from compute shader.
	*/
	gfx::GpuMemoryAllocator* allocator = gfx::GpuMemoryAllocator::GetInstance();

	mDevice->BeginDebugMarker(commandList, "Draw Objects", 1.0f, 1.0f, 1.0f, 1.0f);

	// offset is used to keep track of the buffer offset of transform data
	// so as we don't have to use absolute index but relative
	// We have all transform and material data in same buffer loaded at one
	// and the buffer is binded relatively according to the batch

	uint32_t offset = 0;
	for (auto& batch : mRenderBatches)
	{
		DrawBatch(commandList, batch, offset, allocator);
		// NOTE: for now the size of DrawCommand, Transform and Material is same
		// We need to create offset within the batch later if we decide to share
		// materials
		offset += (uint32_t)batch.drawCommands.size();
	}
	mDevice->EndDebugMarker(commandList);

	auto& envMap = mScene->GetEnvironmentMap();
	gfx::GPUTexture* cubemap = envMap->GetCubemap().get();
	
	mDevice->BeginDebugMarker(commandList, "Draw Cubemap", 1.0f, 1.0f, 1.0f, 1.0f);
	DrawCubemap(commandList, envMap->GetCubemap().get());
	mDevice->EndDebugMarker(commandList);

	mDevice->EndRenderPass(commandList);
	Profiler::EndRangeGPU(commandList, hdrPass);

	if (mEnableBloom)
	{
		auto bloomPass = Profiler::StartRangeGPU(commandList, "Bloom Pass");
		// Bloom Pass
		mBloomFX->Generate(commandList, &mHdrFramebuffer->attachments[1], mBloomBlurRadius);
		mBloomFX->Composite(commandList, &mHdrFramebuffer->attachments[0], mBloomStrength);
		Profiler::EndRangeGPU(commandList, bloomPass);
	}
}

gfx::GPUTexture* Renderer::GetOutputTexture(OutputTextureType colorTextureType)
{
	switch (colorTextureType)
	{
	case OutputTextureType::HDROutput:
		return &mHdrFramebuffer->attachments[0];
	case OutputTextureType::HDRBrightTexture:
		return &mHdrFramebuffer->attachments[1];
	case OutputTextureType::BloomUpSample:
		return mBloomFX->GetTexture();
	case OutputTextureType::HDRDepth:
		return &mHdrFramebuffer->attachments[2];
	default:
		assert(!"Undefined output texture");
		return nullptr;
	}
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
	bufferDesc.usage = gfx::Usage::Upload;
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
	MeshDataComponent* meshData = mScene->mComponentManager->GetComponent<MeshDataComponent>(mScene->mPrimitives);
	const Mesh* mesh = meshData->GetMesh(mScene->mCubeMeshId);

	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[3] = {};
	descriptorInfos[0].resource = mGlobalUniformBuffer.get();
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].size = sizeof(GlobalUniformData);
	descriptorInfos[0].type = gfx::DescriptorType::UniformBuffer;

	gfx::GpuMemoryAllocator* allocator = gfx::GpuMemoryAllocator::GetInstance();
	auto vb = meshData->vertexBuffer;
	auto ib = meshData->indexBuffer;

	descriptorInfos[1].resource = vb;
	descriptorInfos[1].offset = mesh->vertexOffset;
	descriptorInfos[1].size = mesh->vertexCount * sizeof(Vertex);
	descriptorInfos[1].type = gfx::DescriptorType::StorageBuffer;

	descriptorInfos[2].resource = cubemap;
	descriptorInfos[2].offset = 0;
	descriptorInfos[2].type = gfx::DescriptorType::Image;

	mDevice->UpdateDescriptor(mCubemapPipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(commandList, mCubemapPipeline.get());
	mDevice->BindIndexBuffer(commandList, ib);
	mDevice->DrawIndexed(commandList, mesh->indexCount, 1, mesh->indexOffset / sizeof(uint32_t));
}

void Renderer::DrawBatch(gfx::CommandList* commandList, RenderBatch& batch, uint32_t lastOffset, gfx::GpuMemoryAllocator* allocator)
{
	// Copy PerObjectDrawData to the buffer
	//PerObjectData* objectDataPtr = static_cast<PerObjectData*>(mPerObjectDataBuffer->mappedDataPtr) + lastOffset;
	glm::mat4* transformPtr = static_cast<glm::mat4*>(mTransformBuffer->mappedDataPtr) + lastOffset;
	MaterialComponent* materialCompPtr = static_cast<MaterialComponent*>(mMaterialBuffer->mappedDataPtr) + lastOffset;
	gfx::DrawIndirectCommand* drawCommandPtr = static_cast<gfx::DrawIndirectCommand*>(mDrawIndirectBuffer->mappedDataPtr) + lastOffset;

	uint32_t transformCount = (uint32_t)batch.transforms.size();
	std::memcpy(transformPtr, batch.transforms.data(), transformCount * sizeof(glm::mat4));

	uint32_t materialCount = (uint32_t)batch.materials.size();
	std::memcpy(materialCompPtr, batch.materials.data(), materialCount * sizeof(MaterialComponent));

	uint32_t drawCommandCount = (uint32_t)batch.drawCommands.size();
	std::memcpy(drawCommandPtr, batch.drawCommands.data(), drawCommandCount * sizeof(gfx::DrawIndirectCommand));

	gfx::BufferView& vbView = batch.vertexBuffer;
	gfx::BufferView& ibView = batch.indexBuffer;

	auto vb = vbView.buffer;
	auto ib = ibView.buffer;

	auto& envMap = mScene->GetEnvironmentMap();

	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[9] = {};

	descriptorInfos[0] = { mGlobalUniformBuffer.get(), 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };

	descriptorInfos[1] = { vb, 0, vb->desc.size,gfx::DescriptorType::StorageBuffer };

	descriptorInfos[2] = { mTransformBuffer.get(), (uint32_t)(lastOffset * sizeof(glm::mat4)), transformCount * (uint32_t)sizeof(glm::mat4), gfx::DescriptorType::StorageBuffer};

	descriptorInfos[3] = { mMaterialBuffer.get(), (uint32_t)(lastOffset * sizeof(MaterialComponent)), materialCount * (uint32_t)sizeof(MaterialComponent), gfx::DescriptorType::StorageBuffer};

	descriptorInfos[4] = { envMap->GetIrradianceMap().get(), 0, 0, gfx::DescriptorType::Image };

	descriptorInfos[5] = { envMap->GetPrefilterMap().get(), 0, 0, gfx::DescriptorType::Image };

	descriptorInfos[6] = { envMap->GetBRDFLUT().get(), 0, 0, gfx::DescriptorType::Image };

	gfx::DescriptorInfo imageArrInfo = {};
	imageArrInfo.resource = batch.textures.data();
	imageArrInfo.offset = 0;
	imageArrInfo.size = 64;
	imageArrInfo.type = gfx::DescriptorType::ImageArray;
	descriptorInfos[7] = imageArrInfo;

	mDevice->UpdateDescriptor(mTrianglePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(commandList, mTrianglePipeline.get());

	mDevice->BindIndexBuffer(commandList, ib);
	mDevice->DrawIndexedIndirect(commandList, mDrawIndirectBuffer.get(), lastOffset * sizeof(gfx::DrawIndirectCommand), drawCommandCount, sizeof(gfx::DrawIndirectCommand));
}

void Renderer::CreateBatch()
{
	// Clear previous batch
	mRenderBatches.clear();

	std::vector<DrawData> drawDatas;
	mScene->GenerateDrawData(drawDatas);

    // Sort the DrawData according to bufferIndex
	std::sort(drawDatas.begin(), drawDatas.end(), [](const DrawData& lhs, const DrawData& rhs) {
		return lhs.vertexBuffer.bufferIndex < rhs.vertexBuffer.bufferIndex;
	});

	uint32_t lastBufferIndex = ~0u;
	RenderBatch* activeBatch = nullptr;
	if (drawDatas.size() > 0)
	{
		auto addUnique = [](RenderBatch* activeBatch, gfx::GPUTexture* texture) -> uint32_t {
			std::array<gfx::GPUTexture, 64>& textureList = activeBatch->textures;
			uint32_t& textureCount = activeBatch->textureCount;
			auto found = std::find_if(textureList.begin(), textureList.end(), [texture](const gfx::GPUTexture& current) {
				return texture->internalState == current.internalState;
				});
			if (found != textureList.end())
				return (uint32_t)(std::distance(textureList.begin(), found));

			textureList[textureCount++] = *texture;
			return textureCount - 1;
		};

		for (auto& drawData : drawDatas)
		{
			/* The maximum texture limit for each DrawIndirect Invocation is 64.
			* We created batch according to that and mesh buffer. If the
			* textureCount that needs to be added to the batch is greater than
			* we can accomodate then we create new batch
			*/
			uint32_t texInBatch = activeBatch == nullptr ? 0 : activeBatch->textureCount;
			uint32_t texInMat = drawData.material->GetTextureCount();

			uint32_t vbIndex = drawData.vertexBuffer.bufferIndex;
			if (vbIndex != lastBufferIndex || activeBatch == nullptr || (texInBatch + texInMat) > 64)
			{
				mRenderBatches.push_back(RenderBatch{});
				activeBatch = &mRenderBatches.back();
				// Initialize by default texture
				std::fill(activeBatch->textures.begin(), activeBatch->textures.end(), *TextureCache::GetDefaultTexture());
				activeBatch->vertexBuffer = drawData.vertexBuffer;
				activeBatch->indexBuffer = drawData.indexBuffer;
				lastBufferIndex = vbIndex;
			}

			// Find texture and assign new index
			MaterialComponent material = *drawData.material;
			for (int i = 0; i < std::size(material.textures); ++i)
			{
				if (IsTextureValid(material.textures[i]))
					material.textures[i] = addUnique(activeBatch, TextureCache::GetByIndex(material.textures[i]));
			}
			activeBatch->materials.push_back(std::move(material));

			// Update transform Data
			activeBatch->transforms.push_back(std::move(drawData.worldTransform));

			// Create DrawCommands
			gfx::DrawIndirectCommand drawCommand = {};
			drawCommand.firstIndex = drawData.indexBuffer.offset / sizeof(uint32_t);
			drawCommand.indexCount = drawData.indexCount;
			drawCommand.instanceCount = 1;
			drawCommand.vertexOffset = drawData.vertexBuffer.offset / sizeof(Vertex);
			activeBatch->drawCommands.push_back(std::move(drawCommand));

			assert(activeBatch->transforms.size() == activeBatch->drawCommands.size());
			assert(activeBatch->transforms.size() == activeBatch->materials.size());
		}
	}
}
