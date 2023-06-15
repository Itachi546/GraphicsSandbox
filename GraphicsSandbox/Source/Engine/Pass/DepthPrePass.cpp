#include "DepthPrePass.h"

#include "../Scene.h"
#include "../StringConstants.h"
#include "../Utils.h"
#include "../Renderer.h"

gfx::DepthPrePass::DepthPrePass(Renderer* renderer_) : renderer(renderer_)
{
}

void gfx::DepthPrePass::Initialize(RenderPassHandle renderPass)
{
	PipelineDesc pipelineDesc = {};
	uint32_t size = 0;
	char* code = Utils::ReadFile(StringConstants::MAIN_VERT_PATH, &size);
	ShaderDescription shader = { code, size };

	pipelineDesc.shaderCount = 1;
	pipelineDesc.shaderDesc = &shader;

	pipelineDesc.renderPass = renderPass;
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.enableDepthClamp = true;
	pipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);

	descriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };

	// Fill this later while rendering
	descriptorInfos[1] = { INVALID_BUFFER, ~0u, ~0u, gfx::DescriptorType::StorageBuffer };
	descriptorInfos[2] = { INVALID_BUFFER, ~0u, ~0u, gfx::DescriptorType::StorageBuffer };

	delete[] code;
}

void gfx::DepthPrePass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	//Bind Pipeline
	const std::vector<RenderBatch>& renderBatches = renderer->mDrawBatches;

	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle drawIndirectBuffer = renderer->mDrawIndirectBuffer;

	for (auto& batch : renderBatches)
	{
		if (batch.count == 0) continue;

		const gfx::BufferView& vbView = batch.vertexBuffer;
		const gfx::BufferView& ibView = batch.indexBuffer;

		descriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };

		descriptorInfos[2] = { transformBuffer, batch.offset * sizeof(glm::mat4), batch.count * sizeof(glm::mat4), gfx::DescriptorType::StorageBuffer };

		device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));
		device->BindPipeline(commandList, pipeline);

		device->BindIndexBuffer(commandList, ibView.buffer);

		device->DrawIndexedIndirect(commandList, drawIndirectBuffer, batch.offset * sizeof(gfx::DrawIndirectCommand), (uint32_t)batch.count, sizeof(gfx::DrawIndirectCommand));
	}

}

void gfx::DepthPrePass::Shutdown()
{
	gfx::GetDevice()->Destroy(pipeline);
}
/*
void gfx::DepthPrePass::OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height)
{
}
*/