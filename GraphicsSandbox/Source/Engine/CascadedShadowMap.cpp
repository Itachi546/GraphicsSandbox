#include "CascadedShadowMap.h"
#include "Utils.h"
#include "GraphicsDevice.h"
#include "StringConstants.h"
#include "Camera.h"

CascadedShadowMap::CascadedShadowMap() : mDevice(gfx::GetDevice())
{
	{
		gfx::RenderPassDesc renderPassDesc;
		renderPassDesc.attachmentCount = 1;
		gfx::Attachment attachment = { 0, gfx::Format::D32_SFLOAT, gfx::ImageLayout::DepthStencilAttachmentOptimal, kNumCascades};
		renderPassDesc.attachments = &attachment;
		renderPassDesc.hasDepthAttachment = true;
		renderPassDesc.width = kShadowDims;
		renderPassDesc.height = kShadowDims;

		mRenderPass = std::make_unique<gfx::RenderPass>();
		mDevice->CreateRenderPass(&renderPassDesc, mRenderPass.get());
	}

	{
		gfx::PipelineDesc pipelineDesc = {};
		pipelineDesc.rasterizationState.cullMode = gfx::CullMode::Front;
		pipelineDesc.renderPass = mRenderPass.get();

		gfx::ShaderDescription shaders[3] = {};

		uint32_t size = 0;
		{
			char* code = Utils::ReadFile(StringConstants::SHADOW_VERT_PATH, &size);
			shaders[0] = { code, size };
		}
		{
			char* code = Utils::ReadFile(StringConstants::SHADOW_GEOM_PATH, &size);
			shaders[1] = { code, size };
		}
		{
			char* code = Utils::ReadFile(StringConstants::SHADOW_FRAG_PATH, &size);
			shaders[2] = { code, size };
		}

		pipelineDesc.shaderCount = (uint32_t)std::size(shaders);
		pipelineDesc.shaderDesc = shaders;
		pipelineDesc.rasterizationState.enableDepthTest = true;
		pipelineDesc.rasterizationState.enableDepthWrite = true;

		mPipeline = std::make_unique<gfx::Pipeline>();
		mDevice->CreateGraphicsPipeline(&pipelineDesc, mPipeline.get());

		for (uint32_t i = 0; i < std::size(shaders); ++i)
			delete[] shaders[i].code;
	}

	{
		mFramebuffer = std::make_unique<gfx::Framebuffer>();
		mDevice->CreateFramebuffer(mRenderPass.get(), mFramebuffer.get());
	}

	{
		gfx::GPUBufferDesc bufferDesc;
		bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
		bufferDesc.size = sizeof(CascadeData);
		bufferDesc.usage = gfx::Usage::Upload;
		mBuffer = std::make_unique<gfx::GPUBuffer>();
		mDevice->CreateBuffer(&bufferDesc, mBuffer.get());
	}

	CalculateSplitDistance();
}

void CascadedShadowMap::Update(Camera* camera, const glm::vec3& lightDirection)
{
	auto frustumPoints = camera->mFrustumPoints;
	float lastSplitDistance = 0.0f;

	for (int j = 0; j < kNumCascades; ++j)
	{
		float splitDistance = mSplitDistance[j];
		glm::vec3 frustumCorners[8] = {};
		for (int i = 0; i < 4; ++i)
		{
			glm::vec3 direction = glm::normalize(frustumPoints[i + 4] - frustumPoints[i]);
			frustumCorners[i + 4] = frustumPoints[i] + direction * splitDistance;
			frustumCorners[i]     = frustumPoints[i] + direction * lastSplitDistance;
		}
	}

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
		mSplitDistance[i] = (d - kNearDistance);
	}
}