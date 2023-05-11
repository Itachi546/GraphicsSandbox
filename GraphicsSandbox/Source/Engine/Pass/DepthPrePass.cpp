#include "DepthPrePass.h"

#include "../Scene.h"
#include "../StringConstants.h"
#include "../Utils.h"
#include "../Renderer.h"

gfx::DepthPrePass::DepthPrePass(RenderPassHandle renderPass, Renderer* renderer_)
{
	renderer = renderer_;

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

	descriptorInfos[0] = descriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };

	// Fill this later while rendering
	descriptorInfos[1] = { INVALID_BUFFER, ~0u, ~0u, gfx::DescriptorType::StorageBuffer };
	descriptorInfos[2] = { INVALID_BUFFER, ~0u, ~0u, gfx::DescriptorType::StorageBuffer };

	delete[] code;
}

void gfx::DepthPrePass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	std::vector<DrawData>& drawDatas = scene->GetStaticDrawData();

	//Bind Pipeline
	std::vector<RenderBatch> renderBatches;

	// Create Batch
	renderer->CreateBatch(drawDatas, renderBatches);

	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle drawIndirectBuffer = renderer->mDrawIndirectBuffer;

	uint32_t lastOffset = 0;
	for (auto& batch : renderBatches)
	{
		if (batch.drawCommands.size() == 0) continue;

		gfx::BufferView& vbView = batch.vertexBuffer;
		gfx::BufferView& ibView = batch.indexBuffer;

		descriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };

		descriptorInfos[2] = { transformBuffer, (uint32_t)(lastOffset * sizeof(glm::mat4)), (uint32_t)(batch.transforms.size() * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };

		device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));
		device->BindPipeline(commandList, pipeline);

		device->BindIndexBuffer(commandList, ibView.buffer);

		device->DrawIndexedIndirect(commandList, drawIndirectBuffer, lastOffset * sizeof(gfx::DrawIndirectCommand), (uint32_t)batch.drawCommands.size(), sizeof(gfx::DrawIndirectCommand));

		lastOffset += (uint32_t)batch.drawCommands.size();
	}

}

void gfx::DepthPrePass::Shutdown()
{
	gfx::GetDevice()->Destroy(pipeline);
}
