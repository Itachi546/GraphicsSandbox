#include "Renderer.h"
#include "GraphicsDevice.h"
#include "Utils.h"
#include "Scene.h"
#include "Input.h"
#include "Logger.h"

#include <vector>

Renderer::Renderer() : mDevice(gfx::GetDevice())
{
	initializeBuffers();
	mDevice->CreateSemaphore(&mAcquireSemaphore);
	mDevice->CreateSemaphore(&mReleaseSemaphore);

	gfx::RenderPassDesc desc;
	desc.width = 1920;
	desc.height = 1080;
	gfx::Attachment attachments[2] = 
	{
		gfx::Attachment{0, gfx::Format::R16B16G16A16_SFLOAT, gfx::ImageLayout::ColorAttachmentOptimal},
		gfx::Attachment{ 1, mDepthFormat, gfx::ImageLayout::DepthStencilAttachmentOptimal },
	};
	desc.attachmentCount = static_cast<uint32_t>(std::size(attachments));
	desc.attachments = attachments;
	desc.hasDepthAttachment = true;
	mHdrRenderPass = std::make_shared<gfx::RenderPass>();
	mDevice->CreateRenderPass(&desc, mHdrRenderPass.get());

	mHdrFramebuffer = std::make_shared<gfx::Framebuffer>();
	mDevice->CreateFramebuffer(mHdrRenderPass.get(), mHdrFramebuffer.get());

	mTrianglePipeline = loadPipelines("Assets/SPIRV/main.vert.spv", "Assets/SPIRV/main.frag.spv");
	mCubemapPipeline = loadPipelines("Assets/SPIRV/cubemap.vert.spv", "Assets/SPIRV/cubemap.frag.spv", gfx::CullMode::None);


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
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::DepthStencilWrite, gfx::ImageLayout::DepthAttachmentOptimal, &mHdrFramebuffer->attachments[1]},
	};

	gfx::PipelineBarrierInfo colorAttachmentBarrier = { &barriers[0], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::ColorAttachmentOutput};
	gfx::PipelineBarrierInfo depthAttachmentBarrier = { &barriers[1], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::EarlyFramentTest};
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
	if (drawDatas.size() > 0)
	{
		// Copy PerObjectDrawData to the buffer
		PerObjectData* objectDataPtr = static_cast<PerObjectData*>(mPerObjectDataBuffer->mappedDataPtr);
		glm::mat4* transformPtr = static_cast<glm::mat4*>(mTransformBuffer->mappedDataPtr);
		MaterialComponent* materialCompPtr = static_cast<MaterialComponent*>(mMaterialBuffer->mappedDataPtr);

		gfx::DrawIndirectCommand* drawCommandPtr = static_cast<gfx::DrawIndirectCommand*>(mDrawIndirectBuffer->mappedDataPtr);
		for (uint32_t i = 0; i < drawDatas.size(); ++i)
		{

			transformPtr[i] = drawDatas[i].worldTransform;
			objectDataPtr[i].transformIndex = i;
			objectDataPtr[i].materialIndex = i;

			MaterialComponent* mat = drawDatas[i].material;
			materialCompPtr[i].albedo = mat->albedo;
			materialCompPtr[i].roughness = mat->roughness;
			materialCompPtr[i].metallic = mat->metallic;
			materialCompPtr[i].ao = mat->ao;

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

		// TODO: Define static Descriptor beforehand
		gfx::DescriptorInfo descriptorInfos[8] = {};
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
		descriptorInfos[2].size = (uint32_t)drawDatas.size() * sizeof(PerObjectData);
		descriptorInfos[2].type = gfx::DescriptorType::StorageBuffer;

		gfx::GPUBuffer* transformBuffer = mTransformBuffer.get();
		descriptorInfos[3].resource = transformBuffer;
		descriptorInfos[3].offset = 0;
		descriptorInfos[3].size = (uint32_t)drawDatas.size() * sizeof(glm::mat4);
		descriptorInfos[3].type = gfx::DescriptorType::StorageBuffer;

		gfx::GPUBuffer* materialBuffer = mMaterialBuffer.get();
		descriptorInfos[4].resource = materialBuffer;
		descriptorInfos[4].offset = 0;
		descriptorInfos[4].size = (uint32_t)drawDatas.size() * sizeof(MaterialComponent);
		descriptorInfos[4].type = gfx::DescriptorType::StorageBuffer;

		descriptorInfos[5].resource = envMap->GetIrradianceMap().get();
		descriptorInfos[5].type = gfx::DescriptorType::Image;

		descriptorInfos[6].resource = envMap->GetPrefilterMap().get();
		descriptorInfos[6].type = gfx::DescriptorType::Image;

		descriptorInfos[7].resource = envMap->GetBRDFLUT().get();
		descriptorInfos[7].type = gfx::DescriptorType::Image;

		mDevice->UpdateDescriptor(mTrianglePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
		mDevice->BindPipeline(&commandList, mTrianglePipeline.get());

		mDevice->BindIndexBuffer(&commandList, ib);
		mDevice->DrawIndexedIndirect(&commandList, mDrawIndirectBuffer.get(), 0, (uint32_t)drawDatas.size(), sizeof(gfx::DrawIndirectCommand));
	}

	gfx::GPUTexture* cubemap = envMap->GetCubemap().get();

	static int cubemapMode = 0;
	if (Input::Press(Input::Key::KEY_3))
		cubemapMode = 0;
	else if (Input::Press(Input::Key::KEY_4))
		cubemapMode = 1;
	else if (Input::Press(Input::Key::KEY_5))
		cubemapMode = 2;
	

	if(cubemapMode == 0)
		DrawCubemap(&commandList, envMap->GetCubemap().get());
	else if(cubemapMode == 1)
		DrawCubemap(&commandList, envMap->GetIrradianceMap().get());
	else 
		DrawCubemap(&commandList, envMap->GetPrefilterMap().get());
	mDevice->EndRenderPass(&commandList);

	gfx::GPUTexture* outputTexture = &mHdrFramebuffer->attachments[0];
	gfx::ImageBarrierInfo transferSrcBarrier = {gfx::AccessFlag::ColorAttachmentWrite, gfx::AccessFlag::TransferReadBit, gfx::ImageLayout::TransferSrcOptimal, outputTexture};
	gfx::PipelineBarrierInfo transferSrcPipelineBarrier = { &transferSrcBarrier, 1, gfx::PipelineStage::ColorAttachmentOutput, gfx::PipelineStage::TransferBit};
	mDevice->PipelineBarrier(&commandList, &transferSrcPipelineBarrier);

	// Copy the output to swapchain
	mDevice->CopyToSwapchain(&commandList, outputTexture);
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

	pipelineDesc.renderPass = mHdrRenderPass.get();
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
