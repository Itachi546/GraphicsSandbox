#include "CascadedShadowMap.h"
#include "Utils.h"
#include "GraphicsDevice.h"
#include "StringConstants.h"
#include "Camera.h"
#include "DebugDraw.h"

CascadedShadowMap::CascadedShadowMap() : mDevice(gfx::GetDevice())
{
	{
		gfx::RenderPassDesc renderPassDesc;
		renderPassDesc.width = kShadowDims;
		renderPassDesc.height = kShadowDims;

		gfx::GPUTextureDesc depthAttachment = {};
		depthAttachment.bCreateSampler = true;
		depthAttachment.format = gfx::Format::D16_UNORM;
		depthAttachment.bindFlag = gfx::BindFlag::DepthStencil | gfx::BindFlag::ShaderResource;
		depthAttachment.imageAspect = gfx::ImageAspect::Depth;
		depthAttachment.width = kShadowDims;
		depthAttachment.height = kShadowDims;
		depthAttachment.arrayLayers = kNumCascades;
		depthAttachment.imageViewType = gfx::ImageViewType::IV2DArray;

		gfx::Attachment attachments[] = {
			{0, depthAttachment},
		};
		renderPassDesc.attachmentCount = (uint32_t)std::size(attachments);
		renderPassDesc.attachments = attachments;
		renderPassDesc.hasDepthAttachment = true;
		mRenderPass = std::make_unique<gfx::RenderPass>();
		mDevice->CreateRenderPass(&renderPassDesc, mRenderPass.get());
	}

	{
		gfx::PipelineDesc pipelineDesc = {};
		gfx::ShaderDescription shaders[2] = {};
		uint32_t size = 0;
		{
			char* code = Utils::ReadFile(StringConstants::SHADOW_VERT_PATH, &size);
			shaders[0] = { code, size };
		}

		{
			char* code = Utils::ReadFile(StringConstants::SHADOW_GEOM_PATH, &size);
			shaders[1] = { code, size };
		}


		pipelineDesc.shaderCount = (uint32_t)std::size(shaders);
		pipelineDesc.shaderDesc = shaders;

		pipelineDesc.renderPass = mRenderPass.get();
		pipelineDesc.rasterizationState.enableDepthTest = true;
		pipelineDesc.rasterizationState.enableDepthWrite = true;
		pipelineDesc.rasterizationState.cullMode = gfx::CullMode::Front;

		mPipeline = std::make_unique<gfx::Pipeline>();
		mDevice->CreateGraphicsPipeline(&pipelineDesc, mPipeline.get());

		for (uint32_t i = 0; i < std::size(shaders); ++i)
			delete[] shaders[i].code;
	}

	{
		mFramebuffer = std::make_unique<gfx::Framebuffer>();
		mDevice->CreateFramebuffer(mRenderPass.get(), mFramebuffer.get(), kNumCascades);
	}

	{
		gfx::GPUBufferDesc bufferDesc;
		bufferDesc.bindFlag = gfx::BindFlag::ConstantBuffer;
		bufferDesc.size = sizeof(CascadeData);
		bufferDesc.usage = gfx::Usage::Upload;
		mBuffer = std::make_unique<gfx::GPUBuffer>();
		mDevice->CreateBuffer(&bufferDesc, mBuffer.get());
	}

	CalculateSplitDistance();
}

void CascadedShadowMap::Update(Camera* camera, const glm::vec3& lightDirection)
{
	auto cameraFrustum = camera->mFrustumPoints;
	float lastSplitDistance = 0.0f;

	for (int cascade = 0; cascade < kNumCascades; ++cascade)
	{
		float splitDistance = mCascadeData.cascades[cascade].splitDistance.x;
		glm::vec3 thisCascadeFrustum[8] = {};
		// Calculate frustum corner for current split
		glm::vec3 center = glm::vec3(0.0f);
		for (int i = 0; i < 4; ++i)
		{
			glm::vec3 direction = glm::normalize(cameraFrustum[i + 4] - cameraFrustum[i]);
			thisCascadeFrustum[i]     = cameraFrustum[i] + direction * lastSplitDistance;
			thisCascadeFrustum[i + 4] = cameraFrustum[i] + direction * splitDistance;
			center += thisCascadeFrustum[i] + thisCascadeFrustum[i + 4];
		}
		center /= 8.0f;

#if 0
		// Create viewMatrix
		glm::mat4 viewMatrix = glm::lookAt(center + glm::normalize(lightDirection), center, glm::vec3(0.0f, 1.0f, 0.0f));
		// Calculate bounding box in the lightSpace that tightly fit the frustum
		glm::vec3 min = glm::vec3(FLT_MAX);
		glm::vec3 max = glm::vec3(-FLT_MAX);
		for (int i = 0; i < 8; ++i)
		{

			glm::vec3 p = glm::vec3(viewMatrix * glm::vec4(thisCascadeFrustum[i], 1.0f));
			min = glm::min(p, min);
			max = glm::max(p, max);
		}

		// Create OrthoMatrix
		glm::mat4 projectionMatrix = glm::ortho(min.x, max.x, min.y, max.y, min.z, max.z);
		mCascadeData.cascades[cascade].VP = projectionMatrix * viewMatrix;
#else 
		float radius = 0.0f;
		for (int i = 0; i < 8; ++i)
			radius = std::max(radius, glm::length2(center - thisCascadeFrustum[i]));

		radius = std::sqrt(radius);
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::mat4 projection = glm::ortho(-radius, radius, -radius, radius, 0.0f, 2.0f * radius);
		glm::mat4 view = glm::lookAt(center + radius * lightDirection, center, glm::vec3(0.0f, 1.0f, 0.0f));
		mCascadeData.cascades[cascade].VP = projection * view;
#endif
		DebugDraw::AddFrustum(thisCascadeFrustum, 8, colors[cascade]);
		lastSplitDistance = splitDistance;
	}

	// Copy data to uniform buffer
	//mCascadeData.nCascade = (float)kNumCascades;
	std::memcpy(mBuffer->mappedDataPtr, &mCascadeData, sizeof(CascadeData));
}

void CascadedShadowMap::BeginRender(gfx::CommandList* commandList)
{
	gfx::ImageBarrierInfo barrierInfos[] = {
		{gfx::AccessFlag::None, gfx::AccessFlag::DepthStencilWrite, gfx::ImageLayout::DepthAttachmentOptimal, &mFramebuffer->attachments[0]},
	};

	gfx::PipelineBarrierInfo depthBarrier = { &barrierInfos[0], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::EarlyFramentTest};
	mDevice->PipelineBarrier(commandList, &depthBarrier);

	mDevice->BeginRenderPass(commandList, mRenderPass.get(), mFramebuffer.get());
	mDevice->BeginDebugMarker(commandList, "ShadowPass");
}


void CascadedShadowMap::EndRender(gfx::CommandList* commandList)
{
	mDevice->EndDebugMarker(commandList);
	mDevice->EndRenderPass(commandList);
}


void CascadedShadowMap::CalculateSplitDistance()
{
	float clipRange = kShadowDistance - kNearDistance;
	float minZ = kNearDistance;
	float maxZ = kNearDistance + clipRange;
	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

  	for (int i = 0; i < kNumCascades; ++i)
	{
		float p = (i + 1) / float(kNumCascades);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = kSplitLambda * (log - uniform) + uniform;
		mCascadeData.cascades[i].splitDistance = glm::vec4(d - kNearDistance);
	}
}