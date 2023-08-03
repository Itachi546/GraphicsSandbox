#include "CascadedShadowPass.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include "../DebugDraw.h"
#include "../Scene.h"
#include "../Renderer.h"
#include "../MeshData.h"

#include <limits>
namespace gfx {
	CascadedShadowPass::CascadedShadowPass(Renderer* renderer_) : mDevice(gfx::GetDevice()), renderer(renderer_)
	{
		/*
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
		*/
	}

	void CascadedShadowPass::Initialize(RenderPassHandle renderPass)
	{
		gfx::PipelineDesc pipelineDesc = {};
		gfx::ShaderDescription shaders[2] = {};
		uint32_t size = 0;
		{
			char* code = Utils::ReadFile(StringConstants::CSM_VERT_PATH, &size);
			shaders[0] = { code, size };
		}

		{
			char* code = Utils::ReadFile(StringConstants::CSM_GEOM_PATH, &size);
			shaders[1] = { code, size };
		}


		pipelineDesc.shaderCount = (uint32_t)std::size(shaders);
		pipelineDesc.shaderDesc = shaders;

		pipelineDesc.renderPass = renderPass;
		pipelineDesc.rasterizationState.enableDepthTest = true;
		pipelineDesc.rasterizationState.enableDepthWrite = true;
		pipelineDesc.rasterizationState.enableDepthClamp = true;
		pipelineDesc.rasterizationState.cullMode = gfx::CullMode::Front;
		mPipeline = mDevice->CreateGraphicsPipeline(&pipelineDesc);

		for (uint32_t i = 0; i < std::size(shaders); ++i)
			delete[] shaders[i].code;

		{
			gfx::GPUBufferDesc bufferDesc;
			bufferDesc.bindFlag = gfx::BindFlag::ConstantBuffer;
			bufferDesc.size = sizeof(CascadeData);
			bufferDesc.usage = gfx::Usage::Upload;
			mCascadeInfoBuffer = mDevice->CreateBuffer(&bufferDesc);

			bufferDesc.bindFlag = gfx::BindFlag::IndirectBuffer;
			bufferDesc.size = sizeof(DrawIndirectCommand) * 10'000;
		}

		descriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
		descriptorInfos[3] = { mCascadeInfoBuffer, 0, sizeof(CascadeData), gfx::DescriptorType::UniformBuffer };
	}

	void CascadedShadowPass::Render(CommandList* commandList, Scene* scene)
	{
		ecs::Entity sun = scene->GetSun();
		TransformComponent* lightTransform = scene->GetComponentManager()->GetComponent<TransformComponent>(sun);
		glm::mat4 rotationMatrix = lightTransform->GetRotationMatrix();
		glm::vec3 lightDir = rotationMatrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
		lightDir = glm::normalize(lightDir);

		update(scene->GetCamera(), lightDir);

		render(commandList);

	}

	void CascadedShadowPass::render(CommandList* commandList)
	{
		gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
		gfx::BufferHandle dcb = renderer->mIndexedIndirectCommandBuffer;

		//Bind Pipeline
		const std::vector<RenderBatch>& batches = renderer->mDrawBatches;

		uint32_t batchCount = 0;
		for (const auto& batch : batches) {
			const gfx::BufferView& vbView = batch.vertexBuffer;
			descriptorInfos[1] = { vbView.buffer, 0, mDevice->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };
			descriptorInfos[2] = { transformBuffer, (uint32_t)(batch.offset * sizeof(glm::mat4)), (uint32_t)(batch.count * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };

			const gfx::BufferView& ibView = batch.indexBuffer;
			mDevice->UpdateDescriptor(mPipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));
			mDevice->BindPipeline(commandList, mPipeline);

			mDevice->BindIndexBuffer(commandList, ibView.buffer);

			mDevice->DrawIndexedIndirect(commandList,
				dcb,
				batch.offset * sizeof(gfx::DrawIndirectCommand),
				batch.count,
				sizeof(gfx::DrawIndirectCommand));
		}
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

	void CascadedShadowPass::update(Camera* camera, const glm::vec3& lightDirection)
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
		mDevice->CopyToBuffer(mCascadeInfoBuffer, &mCascadeData, 0, sizeof(CascadeData));
	}

	/*
	void CascadedShadowPass::BeginRender(gfx::CommandList* commandList)
	{
		gfx::ImageBarrierInfo barrierInfos[] = {
			{gfx::AccessFlag::None, gfx::AccessFlag::DepthStencilWrite, gfx::ImageLayout::DepthAttachmentOptimal, mAttachment0},
		};

		gfx::PipelineBarrierInfo depthBarrier = { &barrierInfos[0], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::EarlyFramentTest };
		mDevice->PipelineBarrier(commandList, &depthBarrier);

		mDevice->BeginRenderPass(commandList, mRenderPass, mFramebuffer);
		mDevice->BeginDebugMarker(commandList, "ShadowPass");
	}


	void CascadedShadowPass::EndRender(gfx::CommandList* commandList)
	{
		mDevice->EndDebugMarker(commandList);
		mDevice->EndRenderPass(commandList);
	}
	*/
	void CascadedShadowPass::Shutdown()
	{
		mDevice->Destroy(mPipeline);
		mDevice->Destroy(mSkinnedPipeline);
		//mDevice->Destroy(mRenderPass);
		mDevice->Destroy(mCascadeInfoBuffer);
		//mDevice->Destroy(mFramebuffer);
	}

	void CascadedShadowPass::CalculateSplitDistance(Camera* camera)
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
}