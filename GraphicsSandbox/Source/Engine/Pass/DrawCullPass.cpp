#include "DrawCullPass.h"

#include "../Renderer.h"
#include "../Scene.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include <cassert>

namespace gfx {

	DrawCullPass::DrawCullPass(Renderer* renderer_) : renderer(renderer_), pipeline({ gfx::K_INVALID_RESOURCE_HANDLE }) {
		
	}

	void DrawCullPass::Initialize(RenderPassHandle renderPass)
	{
		PipelineDesc desc;
		desc.shaderCount = 1;

		ShaderDescription shaderDesc = {};
		uint32_t sizeInBytes = 0;
		const char* code = Utils::ReadFile(StringConstants::DRAWCULL_COMPUTE_PATH, &sizeInBytes);
		assert(code != nullptr);
		shaderDesc.code = code;
		shaderDesc.sizeInByte = sizeInBytes;
		desc.shaderDesc = &shaderDesc;

		pipeline = gfx::GetDevice()->CreateComputePipeline(&desc);

		delete[] code;
	}

	void DrawCullPass::Render(CommandList* commandList, Scene* scene)
	{
		gfx::GraphicsDevice* device = gfx::GetDevice();

		gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
		gfx::BufferHandle meshDrawDataBuffer = renderer->mMeshDrawDataBuffer;
		gfx::BufferHandle drawIndirectBuffer = renderer->mDrawIndirectBuffer;
		gfx::BufferHandle drawCommandCountBuffer = renderer->mDrawCommandCountBuffer;

		// @TODO avoid copy if possible
		std::vector<RenderBatch> renderBatches = renderer->mDrawBatches;
		const std::vector<RenderBatch>& transparentBatches = renderer->mTransparentBatches;
		renderBatches.insert(renderBatches.end(), transparentBatches.begin(), transparentBatches.end());

		const uint32_t mat4Size = sizeof(glm::mat4);
		const uint32_t drawDataSize = sizeof(MeshDrawData);
		const uint32_t drawIndirectSize = sizeof(MeshDrawIndirectCommand);

		std::array<glm::vec4, 6> frustumPlanes;
		scene->GetCamera()->mFrustum->GetPlanes(frustumPlanes);

		device->FillBuffer(commandList, drawCommandCountBuffer, 0, device->GetBufferSize(drawCommandCountBuffer));
		ResourceBarrierInfo drawCountInitialize[] = {
			ResourceBarrierInfo::CreateBufferBarrier(gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, drawCommandCountBuffer, 0, device->GetBufferSize(drawCommandCountBuffer)),
		};

		gfx::PipelineBarrierInfo bufferInitializeBarrier = {
			drawCountInitialize,
			(uint32_t)std::size(drawCountInitialize),
			gfx::PipelineStage::Transfer,
			gfx::PipelineStage::ComputeShader
		};
		device->PipelineBarrier(commandList, &bufferInitializeBarrier);

		device->PushConstants(commandList, pipeline, gfx::ShaderStage::Compute, frustumPlanes.data(), sizeof(glm::vec4) * 6, 0);

		uint32_t sumDIBufferSize = 0;
		uint32_t sumDICBufferSize = 0;

		for (auto& batch : renderBatches) {
			if (batch.count == 0) continue;

			uint32_t diBufferSize = (uint32_t)batch.count * drawIndirectSize;

			descriptorInfos[0] = { transformBuffer, (uint32_t)batch.offset * mat4Size, (uint32_t)batch.count * mat4Size, gfx::DescriptorType::StorageBuffer };
			descriptorInfos[1] = { meshDrawDataBuffer, (uint32_t)batch.offset * drawDataSize, (uint32_t)batch.count * drawDataSize, gfx::DescriptorType::StorageBuffer };
			descriptorInfos[2] = { drawIndirectBuffer, (uint32_t)batch.offset * drawIndirectSize, diBufferSize, gfx::DescriptorType::StorageBuffer };
			descriptorInfos[3] = { drawCommandCountBuffer, batch.id * sizeof(uint32_t), sizeof(uint32_t), gfx::DescriptorType::StorageBuffer};

			device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));
			device->PushConstants(commandList, pipeline, gfx::ShaderStage::Compute, (void*) &batch.count, sizeof(uint32_t), sizeof(glm::vec4) * 6);
			device->BindPipeline(commandList, pipeline);
			device->DispatchCompute(commandList, uint32_t((batch.count + 31) / 32), 1, 1);

			sumDIBufferSize += diBufferSize;
			sumDICBufferSize += sizeof(uint32_t);
		}

		// Add pipeline barrier
		ResourceBarrierInfo barrierInfos[] = { 
			ResourceBarrierInfo::CreateBufferBarrier(gfx::AccessFlag::ShaderWrite, gfx::AccessFlag::DrawCommandRead, drawIndirectBuffer, 0, sumDIBufferSize),
			ResourceBarrierInfo::CreateBufferBarrier(gfx::AccessFlag::ShaderWrite, gfx::AccessFlag::DrawCommandRead, drawCommandCountBuffer, 0, sumDICBufferSize),
		};

		gfx::PipelineBarrierInfo pipelineBarrier = {
			barrierInfos,
			(uint32_t)std::size(barrierInfos),
			gfx::PipelineStage::ComputeShader,
			gfx::PipelineStage::DrawIndirect
		};
		device->PipelineBarrier(commandList, &pipelineBarrier);
	}

	void DrawCullPass::Shutdown()
	{
		gfx::GraphicsDevice* device = gfx::GetDevice();
		device->Destroy(pipeline);
	}
}
