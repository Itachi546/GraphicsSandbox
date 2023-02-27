#include "Renderer.h"
#include "Utils.h"
#include "Scene.h"
#include "Input.h"
#include "Logger.h"
#include "Profiler.h"
#include "FX/Bloom.h"
#include "CascadedShadowMap.h"
#include "StringConstants.h"
#include "DebugDraw.h"

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

	gfx::GPUTextureDesc colorAttachment = {};
	colorAttachment.bCreateSampler = true;
	colorAttachment.format = mHDRColorFormat;
	colorAttachment.bindFlag = gfx::BindFlag::RenderTarget | gfx::BindFlag::ShaderResource | gfx::BindFlag::StorageImage;
	colorAttachment.imageAspect = gfx::ImageAspect::Color;
	colorAttachment.width = desc.width;
	colorAttachment.height = desc.height;


	gfx::GPUTextureDesc depthAttachment = {};
	depthAttachment.bCreateSampler = true;
	depthAttachment.format = mHDRDepthFormat;
	depthAttachment.bindFlag = gfx::BindFlag::DepthStencil | gfx::BindFlag::ShaderResource;
	depthAttachment.imageAspect = gfx::ImageAspect::Depth;
	depthAttachment.width = desc.width;
	depthAttachment.height = desc.height;

	gfx::Attachment attachments[3] = 
	{
		gfx::Attachment{0, colorAttachment},
		gfx::Attachment{2, depthAttachment},
		gfx::Attachment{1, colorAttachment}
	};

	desc.attachmentCount = static_cast<uint32_t>(std::size(attachments));
	desc.attachments = attachments;
	desc.hasDepthAttachment = true;
	mHdrRenderPass = std::make_shared<gfx::RenderPass>();
	mDevice->CreateRenderPass(&desc, mHdrRenderPass.get());

	mHdrFramebuffer = std::make_shared<gfx::Framebuffer>();
	mDevice->CreateFramebuffer(mHdrRenderPass.get(), mHdrFramebuffer.get(), 1);

	mMeshPipeline = loadHDRPipeline(StringConstants::MAIN_VERT_PATH, StringConstants::MAIN_FRAG_PATH);
	mSkinnedMeshPipeline = loadHDRPipeline(StringConstants::SKINNED_VERT_PATH, StringConstants::SKINNED_FRAG_PATH);
	mCubemapPipeline = loadHDRPipeline(StringConstants::CUBEMAP_VERT_PATH, StringConstants::CUBEMAP_FRAG_PATH, gfx::CullMode::None);

	mBloomFX = std::make_shared<fx::Bloom>(mDevice, desc.width, desc.height, mHDRColorFormat);
	mShadowMap = std::make_shared<CascadedShadowMap>();

	mGlobalUniformData.enabledNormalMapping = true;
	mGlobalUniformData.enableCascadeDebug = 0;
}

// TODO: temp width and height variable
void Renderer::Update(float dt)
{
	if (mUpdateBatches)
	{
		std::vector<DrawData> drawDatas;
		mScene->GenerateDrawData(drawDatas);
		CreateBatch(drawDatas, mRenderBatches);
		mUpdateBatches = false;
	}

	auto compMgr = mScene->GetComponentManager();
	// Generate light Direction
	glm::mat3 transform = compMgr->GetComponent<TransformComponent>(mScene->GetSun())->GetRotationMatrix();

	glm::vec3 direction = normalize(transform * glm::vec3(0.0f, 1.0f, 0.0f));
	Camera* camera = mScene->GetCamera();
	mShadowMap->Update(camera, direction);
	DebugDraw::AddLine(direction * 5.0f, direction, 0xff0000);

	// Update Global Uniform Data
	glm::mat4 P = camera->GetProjectionMatrix();
	mGlobalUniformData.P = P;
	glm::mat4 V = camera->GetViewMatrix();
	mGlobalUniformData.V = V;
	mGlobalUniformData.VP = P * V;
	mGlobalUniformData.cameraPosition = camera->GetPosition();
	mGlobalUniformData.dt += dt;
	mGlobalUniformData.bloomThreshold = mBloomThreshold;

	auto lightArrComponent = compMgr->GetComponentArray<LightComponent>();
	std::vector<LightComponent>& lights = lightArrComponent->components;
	std::vector<ecs::Entity>& entities = lightArrComponent->entities;

	for (int i = 0; i < lights.size(); ++i)
	{
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(entities[i]);
		glm::vec3 position = glm::vec3(0.0f);

		if (lights[i].type == LightType::Directional)
			position = normalize(transform->GetRotationMatrix() * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
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

void Renderer::DrawShadowBatch(gfx::CommandList* commandList, RenderBatch& batch, gfx::Pipeline* pipeline, uint32_t lastOffset)
{
	auto cascadeInfoBuffer = mShadowMap->mBuffer.get();
	gfx::BufferView& vbView = batch.vertexBuffer;
	gfx::BufferView& ibView = batch.indexBuffer;

	std::shared_ptr<gfx::GPUBuffer> vb = vbView.buffer;
	std::shared_ptr<gfx::GPUBuffer> ib = ibView.buffer;

	gfx::DescriptorInfo descriptorInfos[3] = {};
	descriptorInfos[0] = { cascadeInfoBuffer, 0, cascadeInfoBuffer->desc.size,gfx::DescriptorType::UniformBuffer };
	descriptorInfos[1] = { vb.get(), 0, vb->desc.size,gfx::DescriptorType::StorageBuffer };
	descriptorInfos[2] = { mTransformBuffer.get(), (uint32_t)(lastOffset * sizeof(glm::mat4)), (uint32_t)(batch.transforms.size() * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };

	mDevice->UpdateDescriptor(pipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(commandList, pipeline);

	mDevice->BindIndexBuffer(commandList, ib.get());
	mDevice->DrawIndexedIndirect(commandList, mDrawIndirectBuffer.get(), lastOffset * sizeof(gfx::DrawIndirectCommand), (uint32_t)batch.drawCommands.size(), sizeof(gfx::DrawIndirectCommand));
}

void Renderer::DrawSkinnedShadow(gfx::CommandList* commandList)
{
	std::vector<DrawData> drawDatas;
	mScene->GenerateSkinnedMeshDrawData(drawDatas);

	gfx::Pipeline* pipeline = mShadowMap->mSkinnedPipeline.get();
	auto cascadeInfoBuffer = mShadowMap->mBuffer.get();

	auto compMgr = mScene->GetComponentManager();
	// @TODO Maybe we can reuse the same data that is already uploaded to GPU for shadow pass
	// in the main rendering pass also.
	uint32_t skinnedBufferOffset = 0;
	for (auto& drawData : drawDatas)
	{
		gfx::BufferView& vbView = drawData.vertexBuffer;
		gfx::BufferView& ibView = drawData.indexBuffer;

		auto& vb = vbView.buffer;
		auto& ib = ibView.buffer;

		auto skinnedMeshComp = compMgr->GetComponent<SkinnedMeshRenderer>(drawData.entity);
		std::vector<glm::mat4> skinnedMatrix;
		skinnedMeshComp->skeleton.GetAnimatedPose().GetMatrixPallete(skinnedMatrix);
		for (uint32_t i = 0; i < skinnedMatrix.size(); ++i)
			skinnedMatrix[i] = drawData.worldTransform * skinnedMatrix[i] * skinnedMeshComp->skeleton.GetInvBindPose(i);

		assert(skinnedMatrix.size() <= MAX_BONE_COUNT);

		uint32_t dataSize = static_cast<uint32_t>(skinnedMatrix.size() * sizeof(glm::mat4));
		glm::mat4* ptr = static_cast<glm::mat4*>(mSkinnedMatrixBuffer->mappedDataPtr) + (skinnedBufferOffset / sizeof(glm::mat4));
		std::memcpy(ptr, skinnedMatrix.data(), dataSize);

		// TODO: Define static Descriptor beforehand
		gfx::DescriptorInfo descriptorInfos[3] = {};

		descriptorInfos[0] = { cascadeInfoBuffer, 0, cascadeInfoBuffer->desc.size,gfx::DescriptorType::UniformBuffer };
		descriptorInfos[1] = { vb.get(), 0, vb->desc.size,gfx::DescriptorType::StorageBuffer };
		descriptorInfos[2] = { mSkinnedMatrixBuffer.get(), skinnedBufferOffset, dataSize,gfx::DescriptorType::UniformBuffer };

		mDevice->UpdateDescriptor(pipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
		mDevice->BindPipeline(commandList, pipeline);

		mDevice->BindIndexBuffer(commandList, ib.get());
		mDevice->DrawIndexed(commandList, drawData.indexCount, 1, drawData.indexBuffer.offset);

		skinnedBufferOffset += dataSize;
	}
}


void Renderer::DrawShadowMap(gfx::CommandList* commandList)
{
	// BeginShadow Pass
	auto rangeId = Profiler::StartRangeGPU(commandList, "Shadow Pass");
	mShadowMap->BeginRender(commandList);

	uint32_t lastOffset = 0;
	auto pipeline = mShadowMap->mPipeline.get();
	for (auto& batch : mRenderBatches)
	{
		DrawShadowBatch(commandList, batch, pipeline, lastOffset);
		lastOffset += (uint32_t)batch.drawCommands.size();
	}

	DrawSkinnedShadow(commandList);

	mShadowMap->EndRender(commandList);
	Profiler::EndRangeGPU(commandList, rangeId);
}

void Renderer::Render(gfx::CommandList* commandList)
{
	{
		// Draw Shadow Map
		DrawShadowMap(commandList);

		gfx::ImageBarrierInfo shadowBarrier = {
				gfx::ImageBarrierInfo{gfx::AccessFlag::DepthStencilWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, &mShadowMap->mFramebuffer->attachments[0]},
		};
		gfx::PipelineBarrierInfo barrier = { &shadowBarrier, 1, gfx::PipelineStage::LateFramentTest, gfx::PipelineStage::FragmentShader};
		mDevice->PipelineBarrier(commandList, &barrier);
	}

	// Begin HDR RenderPass
	{
		gfx::ImageBarrierInfo barriers[] = {
			gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ColorAttachmentWrite, gfx::ImageLayout::ColorAttachmentOptimal, &mHdrFramebuffer->attachments[0]},
			gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ColorAttachmentWrite, gfx::ImageLayout::ColorAttachmentOptimal, &mHdrFramebuffer->attachments[1]},
			gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::DepthStencilWrite, gfx::ImageLayout::DepthAttachmentOptimal, &mHdrFramebuffer->attachments[2]},
		};

		gfx::PipelineBarrierInfo colorAttachmentBarrier = { barriers, 2, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::ColorAttachmentOutput };
		gfx::PipelineBarrierInfo depthAttachmentBarrier = { &barriers[2], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::EarlyFramentTest };
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
		mDevice->BeginDebugMarker(commandList, "Draw Objects", 1.0f, 1.0f, 1.0f, 1.0f);

		// offset is used to keep track of the buffer offset of transform data
		// so as we don't have to use absolute index but relative
		// We have all transform and material data in same buffer loaded at one
		// and the buffer is binded relatively according to the batch

		uint32_t offset = 0;
		for (auto& batch : mRenderBatches)
		{
			DrawBatch(commandList, batch, offset, mMeshPipeline.get());
			// NOTE: for now the size of DrawCommand, Transform and Material is same
			// We need to create offset within the batch later if we decide to share
			// materials
			offset += (uint32_t)batch.drawCommands.size();
		}

		// Draw Skinned Mesh
		DrawSkinnedMesh(commandList, offset);

		mDevice->EndDebugMarker(commandList);

		// Draw Cubemap
		auto& envMap = mScene->GetEnvironmentMap();
		gfx::GPUTexture* cubemap = envMap->GetCubemap().get();

		mDevice->BeginDebugMarker(commandList, "Draw Cubemap", 1.0f, 1.0f, 1.0f, 1.0f);
		DrawCubemap(commandList, envMap->GetCubemap().get());
		mDevice->EndDebugMarker(commandList);
		mDevice->EndRenderPass(commandList);
		Profiler::EndRangeGPU(commandList, hdrPass);
	}

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

	// Skinned Matrix Buffer;
	uniformBufferDesc.size = sizeof(glm::mat4) * MAX_BONE_COUNT * 10;
	mSkinnedMatrixBuffer = std::make_shared<gfx::GPUBuffer>();
	mDevice->CreateBuffer(&uniformBufferDesc, mSkinnedMatrixBuffer.get());

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
	MeshRenderer* meshRenderer = mScene->mComponentManager->GetComponent<MeshRenderer>(mScene->mCube);

	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[3] = {};
	descriptorInfos[0].resource = mGlobalUniformBuffer.get();
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].size = sizeof(GlobalUniformData);
	descriptorInfos[0].type = gfx::DescriptorType::UniformBuffer;

	gfx::GpuMemoryAllocator* allocator = gfx::GpuMemoryAllocator::GetInstance();
	auto& vb = meshRenderer->vertexBuffer;
	auto& ib = meshRenderer->indexBuffer;

	descriptorInfos[1].resource = vb.buffer.get();
	descriptorInfos[1].offset = vb.offset;
	descriptorInfos[1].size = vb.size;
	descriptorInfos[1].type = gfx::DescriptorType::StorageBuffer;

	descriptorInfos[2].resource = cubemap;
	descriptorInfos[2].offset = 0;
	descriptorInfos[2].type = gfx::DescriptorType::Image;

	mDevice->UpdateDescriptor(mCubemapPipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(commandList, mCubemapPipeline.get());
	mDevice->BindIndexBuffer(commandList, ib.buffer.get());
	mDevice->DrawIndexed(commandList, meshRenderer->GetIndexCount(), 1, ib.offset / sizeof(uint32_t));
}

void Renderer::DrawBatch(gfx::CommandList* commandList, RenderBatch& batch, uint32_t lastOffset, gfx::Pipeline* pipeline)
{
	gfx::BufferView& vbView = batch.vertexBuffer;
	gfx::BufferView& ibView = batch.indexBuffer;

	auto vb = vbView.buffer;
	auto ib = ibView.buffer;

	auto& envMap = mScene->GetEnvironmentMap();

	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[10] = {};

	descriptorInfos[0] = { mGlobalUniformBuffer.get(), 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };

	descriptorInfos[1] = { vb.get(), 0, vb->desc.size,gfx::DescriptorType::StorageBuffer};

	descriptorInfos[2] = { mTransformBuffer.get(), (uint32_t)(lastOffset * sizeof(glm::mat4)), (uint32_t)(batch.transforms.size() * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer};

	descriptorInfos[3] = { mMaterialBuffer.get(), (uint32_t)(lastOffset * sizeof(MaterialComponent)), (uint32_t)(batch.materials.size() * (uint32_t)sizeof(MaterialComponent)), gfx::DescriptorType::StorageBuffer};

	descriptorInfos[4] = { envMap->GetIrradianceMap().get(), 0, 0, gfx::DescriptorType::Image };

	descriptorInfos[5] = { envMap->GetPrefilterMap().get(), 0, 0, gfx::DescriptorType::Image };

	descriptorInfos[6] = { envMap->GetBRDFLUT().get(), 0, 0, gfx::DescriptorType::Image };

	descriptorInfos[7] = mShadowMap->GetCascadeBufferDescriptor();
	descriptorInfos[8] = mShadowMap->GetShadowMapDescriptor();

	gfx::DescriptorInfo imageArrInfo = {};
	imageArrInfo.resource = batch.textures.data();
	imageArrInfo.offset = 0;
	imageArrInfo.size = 64;
	imageArrInfo.type = gfx::DescriptorType::ImageArray;
	descriptorInfos[9] = imageArrInfo;

	mDevice->UpdateDescriptor(pipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(commandList, pipeline);

	mDevice->BindIndexBuffer(commandList, ib.get());
	mDevice->DrawIndexedIndirect(commandList, mDrawIndirectBuffer.get(), lastOffset * sizeof(gfx::DrawIndirectCommand), (uint32_t)batch.drawCommands.size(), sizeof(gfx::DrawIndirectCommand));
}

// Offset parameter is used to reuse the Material Buffer
void Renderer::DrawSkinnedMesh(gfx::CommandList* commandList, uint32_t offset)
{
	std::vector<DrawData> drawDatas;
	mScene->GenerateSkinnedMeshDrawData(drawDatas);

	gfx::Pipeline* pipeline = mSkinnedMeshPipeline.get();
	auto compMgr = mScene->GetComponentManager();

	// Copy Materials
	std::array<gfx::GPUTexture, 64> textures;
	std::fill(textures.begin(), textures.end(), *TextureCache::GetDefaultTexture());

	std::vector<MaterialComponent> materials;
	uint32_t textureIndex = 0;
	auto addUnique = [&textures, &textureIndex](gfx::GPUTexture* texture) -> uint32_t {
		auto found = std::find_if(textures.begin(), textures.end(), [texture](const gfx::GPUTexture& current) {
			return texture->internalState == current.internalState;
			});

		if (found != textures.end())
			return (uint32_t)(std::distance(textures.begin(), found));

		textures[textureIndex++] = *texture;
		return textureIndex - 1;
	};

	for (auto& drawData : drawDatas)
	{
		MaterialComponent material = *drawData.material;
		for (int i = 0; i < std::size(material.textures); ++i)
		{
			if (IsTextureValid(material.textures[i]))
				material.textures[i] = addUnique(TextureCache::GetByIndex(material.textures[i]));
		}
		materials.push_back(std::move(material));
	}

	uint32_t materialCount = (uint32_t)materials.size();
	MaterialComponent* materialCompPtr = static_cast<MaterialComponent*>(mMaterialBuffer->mappedDataPtr) + offset;
	std::memcpy(materialCompPtr, materials.data(), materialCount * sizeof(MaterialComponent));

	auto& envMap = mScene->GetEnvironmentMap();

	gfx::DescriptorInfo descriptorInfos[10] = {};
	descriptorInfos[0] = { mGlobalUniformBuffer.get(), 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
	descriptorInfos[4] = { envMap->GetIrradianceMap().get(), 0, 0, gfx::DescriptorType::Image };
	descriptorInfos[5] = { envMap->GetPrefilterMap().get(), 0, 0, gfx::DescriptorType::Image };
	descriptorInfos[6] = { envMap->GetBRDFLUT().get(), 0, 0, gfx::DescriptorType::Image };
	descriptorInfos[7] = mShadowMap->GetCascadeBufferDescriptor();
	descriptorInfos[8] = mShadowMap->GetShadowMapDescriptor();
	gfx::DescriptorInfo imageArrInfo = {};
	imageArrInfo.resource = textures.data();
	imageArrInfo.offset = 0;
	imageArrInfo.size = 64;
	imageArrInfo.type = gfx::DescriptorType::ImageArray;
	descriptorInfos[9] = imageArrInfo;

	uint32_t skinnedBufferOffset = 0;
	uint32_t materialOffset = offset;
	for (auto& drawData : drawDatas)
	{
		gfx::BufferView& vbView = drawData.vertexBuffer;
		gfx::BufferView& ibView = drawData.indexBuffer;

		auto& vb = vbView.buffer;
		auto& ib = ibView.buffer;

		// Copy the skinned matrix to the buffer
		auto skinnedMeshComp = compMgr->GetComponent<SkinnedMeshRenderer>(drawData.entity);
		std::vector<glm::mat4> skinnedMatrix;
		skinnedMeshComp->skeleton.GetAnimatedPose().GetMatrixPallete(skinnedMatrix);
		for (uint32_t i = 0; i < skinnedMatrix.size(); ++i)
			skinnedMatrix[i] = drawData.worldTransform * skinnedMatrix[i] * skinnedMeshComp->skeleton.GetInvBindPose(i);

		assert(skinnedMatrix.size() <= MAX_BONE_COUNT);
		glm::mat4* ptr = static_cast<glm::mat4*>(mSkinnedMatrixBuffer->mappedDataPtr) + (skinnedBufferOffset / sizeof(glm::mat4));

		uint32_t dataSize = static_cast<uint32_t>(skinnedMatrix.size()) * sizeof(glm::mat4);
		std::memcpy(ptr, skinnedMatrix.data(), dataSize);

		// TODO: Define static Descriptor beforehand
		descriptorInfos[1] = { vb.get(), 0, vb->desc.size,gfx::DescriptorType::StorageBuffer };
		descriptorInfos[2] = { mSkinnedMatrixBuffer.get(), skinnedBufferOffset, dataSize, gfx::DescriptorType::UniformBuffer };
		descriptorInfos[3] = { mMaterialBuffer.get(), (uint32_t)(materialOffset * sizeof(MaterialComponent)), (uint32_t)sizeof(MaterialComponent), gfx::DescriptorType::StorageBuffer };

		mDevice->UpdateDescriptor(pipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
		mDevice->BindPipeline(commandList, pipeline);

		mDevice->BindIndexBuffer(commandList, ib.get());
		mDevice->DrawIndexed(commandList, drawData.indexCount, 1, drawData.indexBuffer.offset);

		skinnedBufferOffset += dataSize;
		materialOffset += 1;
	}
}

void Renderer::CreateBatch(std::vector<DrawData>& drawDatas, std::vector<RenderBatch>& renderBatch)
{
	// Clear previous batch
	renderBatch.clear();

    // Sort the DrawData according to bufferIndex
	std::sort(drawDatas.begin(), drawDatas.end(), [](const DrawData& lhs, const DrawData& rhs) {
		return lhs.vertexBuffer.buffer < rhs.vertexBuffer.buffer;
	});

	gfx::GPUBuffer* lastBuffer = nullptr;
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

			gfx::GPUBuffer* buffer = drawData.vertexBuffer.buffer.get();
			if (buffer != lastBuffer || activeBatch == nullptr || (texInBatch + texInMat) > 64)
			{
				renderBatch.push_back(RenderBatch{});
				activeBatch = &renderBatch.back();
				// Initialize by default texture
				std::fill(activeBatch->textures.begin(), activeBatch->textures.end(), *TextureCache::GetDefaultTexture());
				activeBatch->vertexBuffer = drawData.vertexBuffer;
				activeBatch->indexBuffer = drawData.indexBuffer;
				lastBuffer = buffer;
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
			drawCommand.vertexOffset = drawData.vertexBuffer.offset / drawData.elmSize;
			activeBatch->drawCommands.push_back(std::move(drawCommand));

			assert(activeBatch->transforms.size() == activeBatch->drawCommands.size());
			assert(activeBatch->transforms.size() == activeBatch->materials.size());
		}

		// Copy PerObjectDrawData to the buffer
        //PerObjectData* objectDataPtr = static_cast<PerObjectData*>(mPerObjectDataBuffer->mappedDataPtr) + lastOffset;
		uint32_t lastOffset = 0;
		for (auto& batch : renderBatch)
		{
			glm::mat4* transformPtr = static_cast<glm::mat4*>(mTransformBuffer->mappedDataPtr) + lastOffset;
			MaterialComponent* materialCompPtr = static_cast<MaterialComponent*>(mMaterialBuffer->mappedDataPtr) + lastOffset;
			gfx::DrawIndirectCommand* drawCommandPtr = static_cast<gfx::DrawIndirectCommand*>(mDrawIndirectBuffer->mappedDataPtr) + lastOffset;

			uint32_t transformCount = (uint32_t)batch.transforms.size();
			std::memcpy(transformPtr, batch.transforms.data(), transformCount * sizeof(glm::mat4));

			uint32_t materialCount = (uint32_t)batch.materials.size();
			std::memcpy(materialCompPtr, batch.materials.data(), materialCount * sizeof(MaterialComponent));

			uint32_t drawCommandCount = (uint32_t)batch.drawCommands.size();
			std::memcpy(drawCommandPtr, batch.drawCommands.data(), drawCommandCount * sizeof(gfx::DrawIndirectCommand));

			lastOffset += (uint32_t)batch.transforms.size();
		}
	}
}
