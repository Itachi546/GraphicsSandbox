#include "CascadedShadowMap.h"
#include "Utils.h"
#include "GraphicsDevice.h"
#include "StringConstants.h"
#include "Camera.h"
#include "DebugDraw.h"

#include <limits>

CascadedShadowMap::CascadedShadowMap() : mDevice(gfx::GetDevice())
{
	{
		gfx::RenderPassDesc renderPassDesc;
		renderPassDesc.width = kShadowDims;
		renderPassDesc.height = kShadowDims;

		gfx::GPUTextureDesc depthAttachment = {};
		depthAttachment.bCreateSampler = true;
		depthAttachment.format = gfx::Format::D24_UNORM_S8_UINT;
		depthAttachment.bindFlag = gfx::BindFlag::DepthStencil | gfx::BindFlag::ShaderResource;
		depthAttachment.imageAspect = gfx::ImageAspect::Depth;
		depthAttachment.width = kShadowDims;
		depthAttachment.height = kShadowDims;
		depthAttachment.arrayLayers = kNumCascades;
		depthAttachment.mipLevels = 1;
		depthAttachment.imageViewType = gfx::ImageViewType::IV2DArray;
		depthAttachment.samplerInfo.wrapMode = gfx::TextureWrapMode::ClampToBorder;
		depthAttachment.samplerInfo.enableBorder = true;

		std::vector<gfx::Attachment> attachments = {
			{0, depthAttachment},
		};
		renderPassDesc.attachments = attachments;
		renderPassDesc.hasDepthAttachment = true;
		mRenderPass = mDevice->CreateRenderPass(&renderPassDesc);
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

		pipelineDesc.renderPass = mRenderPass;
		pipelineDesc.rasterizationState.enableDepthTest = true;
		pipelineDesc.rasterizationState.enableDepthWrite = true;
		pipelineDesc.rasterizationState.enableDepthClamp = true;
		pipelineDesc.rasterizationState.cullMode = gfx::CullMode::Front;
		mPipeline = mDevice->CreateGraphicsPipeline(&pipelineDesc);

		{
			char* code = Utils::ReadFile(StringConstants::SHADOW_SKINNED_VERT_PATH, &size);
			shaders[0] = { code, size };
		}
		mSkinnedPipeline = mDevice->CreateGraphicsPipeline(&pipelineDesc);

		for (uint32_t i = 0; i < std::size(shaders); ++i)
			delete[] shaders[i].code;
	}

	{
		mFramebuffer = mDevice->CreateFramebuffer(mRenderPass, kNumCascades);
	}

	{
		gfx::GPUBufferDesc bufferDesc;
		bufferDesc.bindFlag = gfx::BindFlag::ConstantBuffer;
		bufferDesc.size = sizeof(CascadeData);
		bufferDesc.usage = gfx::Usage::Upload;
		mBuffer = mDevice->CreateBuffer(&bufferDesc);
	}

	mAttachment0 = mDevice->GetFramebufferAttachment(mFramebuffer, 0);
}

std::array<glm::vec3, 8> CalculateFrustumCorners(glm::mat4 VP)
{
	std::array<glm::vec3, 8> frustumCorners =
	{
		glm::vec3(-1.0f,  1.0f, -1.0f),  // NTL
		glm::vec3(1.0f,  1.0f, -1.0f),   // NTR
		glm::vec3(1.0f, -1.0f, -1.0f),   // NBR
		glm::vec3(-1.0f, -1.0f, -1.0f),  // NBL
		glm::vec3(-1.0f,  1.0f,  1.0f),  // FTL
		glm::vec3(1.0f,  1.0f,  1.0f),   // FTR
		glm::vec3(1.0f, -1.0f,  1.0f),   // FBR
		glm::vec3(-1.0f, -1.0f,  1.0f),  // FBL
	};

	// Project frustum corners into world space
	// inv(view) * inv(projection) * p
	// inv(A) * inv(B) = inv(BA)
	glm::mat4 invCam = glm::inverse(VP);
	for (uint32_t i = 0; i < 8; i++)
	{
		glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
		frustumCorners[i] = invCorner / invCorner.w;
	}

	return frustumCorners;
}

void CascadedShadowMap::Update(Camera* camera, const glm::vec3& lightDirection)
{
	CalculateSplitDistance(camera);

	auto cameraFrustum = camera->mFrustumPoints;
	float lastSplitDistance = camera->GetNearPlane();

	glm::vec3 ld = glm::normalize(lightDirection);
	for (int cascade = 0; cascade < kNumCascades; ++cascade)
	{
		Cascade& currentCascade = mCascadeData.cascades[cascade];
		float splitDistance = currentCascade.splitDistance.x;
		glm::mat4 V = camera->GetViewMatrix();
		glm::mat4 P = glm::perspective(camera->GetFOV(), camera->GetAspect(), lastSplitDistance, splitDistance);
		auto frustumCorners = CalculateFrustumCorners(P * V);

		glm::vec3 center = glm::vec3(0.0f);
		for (int i = 0; i < frustumCorners.size(); ++i)
			center += frustumCorners[i];
		center /= float(frustumCorners.size());

		glm::mat4 lightView = glm::lookAt(center + ld, center, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::vec3 minPoint = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 maxPoint = glm::vec3(std::numeric_limits<float>::lowest());

		float radius = 0.0f;
		for (const auto& v : frustumCorners)
		{
			float distance = glm::length(v - center);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;
		glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, -radius, radius);
		currentCascade.VP = lightProj * lightView;
		lastSplitDistance = splitDistance;
	}

	// Copy data to uniform buffer
	mCascadeData.shadowDims = glm::vec4(kShadowDims, kShadowDims, kNumCascades, kNumCascades);
	mDevice->CopyToBuffer(mBuffer, &mCascadeData, 0, sizeof(CascadeData));
}

void CascadedShadowMap::BeginRender(gfx::CommandList* commandList)
{
	gfx::ImageBarrierInfo barrierInfos[] = {
		{gfx::AccessFlag::None, gfx::AccessFlag::DepthStencilWrite, gfx::ImageLayout::DepthAttachmentOptimal, mAttachment0},
	};

	gfx::PipelineBarrierInfo depthBarrier = { &barrierInfos[0], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::EarlyFramentTest};
	mDevice->PipelineBarrier(commandList, &depthBarrier);

	mDevice->BeginRenderPass(commandList, mRenderPass, mFramebuffer);
	mDevice->BeginDebugMarker(commandList, "ShadowPass");
}


void CascadedShadowMap::EndRender(gfx::CommandList* commandList)
{
	mDevice->EndDebugMarker(commandList);
	mDevice->EndRenderPass(commandList);
}

void CascadedShadowMap::Shutdown()
{
	mDevice->Destroy(mPipeline);
	mDevice->Destroy(mSkinnedPipeline);
	mDevice->Destroy(mRenderPass);
	mDevice->Destroy(mBuffer);
	mDevice->Destroy(mFramebuffer);
}

void CascadedShadowMap::CalculateSplitDistance(Camera* camera)
{
#if 0
	mCascadeData.cascades[0].splitDistance.x = kShadowDistance / 50.0f;
	mCascadeData.cascades[1].splitDistance.x = kShadowDistance / 25.0f;
	mCascadeData.cascades[2].splitDistance.x = kShadowDistance / 10.0f;
	mCascadeData.cascades[3].splitDistance.x = kShadowDistance / 5.0f;
	mCascadeData.cascades[4].splitDistance.x = kShadowDistance;
#else 

	float nearDistance = camera->GetNearPlane();
	float clipRange = kShadowDistance - nearDistance;
	float minZ = nearDistance;
	float maxZ = nearDistance + clipRange;
	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

  	for (int i = 0; i < kNumCascades; ++i)
	{
		float p = (i + 1) / float(kNumCascades);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = kSplitLambda * (log - uniform) + uniform;
		mCascadeData.cascades[i].splitDistance = glm::vec4(d - nearDistance);
	}
#endif
}