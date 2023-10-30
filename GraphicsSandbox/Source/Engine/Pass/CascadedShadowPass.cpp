#include "CascadedShadowPass.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include "../DebugDraw.h"
#include "../Scene.h"
#include "../MeshData.h"
#include "../GUI/ImGuiService.h"

#include <limits>
namespace gfx {
	CascadedShadowPass::CascadedShadowPass(Renderer* renderer_) : mDevice(gfx::GetDevice()), renderer(renderer_)
	{
	}

	void CascadedShadowPass::Initialize(RenderPassHandle renderPass)
	{
		ShaderPathInfo* pathInfo = ShaderPath::get("csm_pass");
		gfx::PipelineDesc pipelineDesc = {};
		gfx::ShaderDescription shaders[2] = {};
		uint32_t size = 0;
		{
			char* code = Utils::ReadFile(pathInfo->shaders[0], &size);
			shaders[0] = { code, size };
		}

		{
			char* code = Utils::ReadFile(pathInfo->shaders[1], &size);
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
			bufferDesc.usage = gfx::Usage::Upload;
			bufferDesc.bindFlag = gfx::BindFlag::IndirectBuffer;
			bufferDesc.size = sizeof(DrawIndirectCommand) * 10'000;
		}

		descriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
		descriptorInfos[3] = { renderer->mCascadeInfoBuffer, 0, sizeof(CascadeData), gfx::DescriptorType::UniformBuffer };
	}

	void CascadedShadowPass::Render(CommandList* commandList, Scene* scene)
	{
		ecs::Entity sun = scene->GetSun();
		LightComponent* lightComponent = scene->GetComponentManager()->GetComponent<LightComponent>(sun);
		if (!lightComponent->enabled) return;

		// Calculate light direction
		TransformComponent* lightTransform = scene->GetComponentManager()->GetComponent<TransformComponent>(sun);
		glm::mat4 rotationMatrix = lightTransform->GetRotationMatrix();
		glm::vec3 lightDir = rotationMatrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
		lightDir = glm::normalize(lightDir);

		update(scene->GetCamera(), lightDir);

		render(commandList);

	}

	void CascadedShadowPass::AddUI()
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Casaded Shadow Map")) {
			ImGui::SliderFloat("Shadow distance", &kShadowDistance, 5.0f, 200.0f);
			ImGui::SliderFloat("Split Factor", &kSplitLambda, 0.0f, 1.0f);
			ImGui::Checkbox("Freeze frustum", &mFreezeCameraFrustum);
			ImGui::Checkbox("Enable Debug Cascade", &mEnableCascadeDebug);
			ImGui::SliderInt("Debug Cascade Index", &debugCascadeIndex, 0, 4);
			ImGui::Text("%.2f/%.2f/%.2f/%.2f/%.2f", mCascadeData.cascades[0].splitDistance.x,
				mCascadeData.cascades[1].splitDistance.x,
				mCascadeData.cascades[2].splitDistance.x,
				mCascadeData.cascades[3].splitDistance.x,
				mCascadeData.cascades[4].splitDistance.x);
		}
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
		if (!mFreezeCameraFrustum) {
			cameraFrustumPoints = camera->mFrustumPoints;
			cameraVP = camera->GetViewMatrix();
		}

		float lastSplitDistance = camera->GetNearPlane();

		glm::vec3 ld = glm::normalize(lightDirection);
		for (int cascade = 0; cascade < kNumCascades; ++cascade)
		{
			Cascade& currentCascade = mCascadeData.cascades[cascade];
			float splitDistance = currentCascade.splitDistance.x;

			glm::mat4 P = glm::perspective(camera->GetFOV(), camera->GetAspect(), lastSplitDistance, splitDistance);
			auto frustumCorners = CalculateFrustumCorners(P * cameraVP);

			glm::vec3 center = glm::vec3(0.0f);
			for (int i = 0; i < frustumCorners.size(); ++i)
				center += frustumCorners[i];
			center /= float(frustumCorners.size());

			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
			if (ld.y > 0.99f)
				up = glm::vec3(0.0f, 0.0f, 1.0f);

			float radius = 0.0f;
			for (const auto& v : frustumCorners)
			{
				float distance = glm::length(v - center);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;
			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::mat4 lightView = glm::lookAt(center - minExtents.z * ld, center, up);
			glm::mat4 lightProj = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);
			currentCascade.VP = lightProj * lightView;

			if (cascade == debugCascadeIndex && mEnableCascadeDebug) {
				auto fp = CalculateFrustumCorners(currentCascade.VP);
				DebugDraw::AddFrustumPrimitive(fp, colors[cascade]);
				DebugDraw::AddFrustum(frustumCorners.data(), 8, 0xffffffff);
			}

			lastSplitDistance = splitDistance;
		}

		// Copy data to uniform buffer
		mCascadeData.shadowDims = glm::vec4(mShadowDims.x, mShadowDims.y, kNumCascades, kNumCascades);
		mDevice->CopyToBuffer(renderer->mCascadeInfoBuffer, &mCascadeData, 0, sizeof(CascadeData));
	}

	void CascadedShadowPass::Shutdown()
	{
		mDevice->Destroy(mPipeline);
		mDevice->Destroy(mSkinnedPipeline);

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