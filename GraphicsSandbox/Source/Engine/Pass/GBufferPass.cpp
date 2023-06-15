#include "GBufferPass.h"

#include "../Renderer.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include "../Scene.h"

gfx::GBufferPass::GBufferPass(Renderer* renderer_) : renderer(renderer_)
{
}

void gfx::GBufferPass::Initialize(RenderPassHandle renderPass)
{
	PipelineDesc pipelineDesc = {};

	ShaderDescription shaders[2];
	uint32_t size = 0;
	char* vertexCode = Utils::ReadFile(StringConstants::MAIN_VERT_PATH, &size);
	shaders[0] = { vertexCode, size };

	char* fragmentCode = Utils::ReadFile(StringConstants::GBUFFER_FRAG_PATH, &size);
	shaders[1] = { fragmentCode, size };

	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders;

	pipelineDesc.renderPass = renderPass;
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.enableDepthClamp = true;

	gfx::BlendState blendStates[5] = {};
	pipelineDesc.blendStates = blendStates;
	pipelineDesc.blendStateCount = 5;
	pipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);

	delete[] vertexCode;
	delete[] fragmentCode;

	descriptorInfos[0] = descriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
	// Fill this later while rendering
	descriptorInfos[1] = { INVALID_BUFFER, ~0u, ~0u, gfx::DescriptorType::StorageBuffer };
	descriptorInfos[2] = { INVALID_BUFFER, ~0u, ~0u, gfx::DescriptorType::StorageBuffer };
	descriptorInfos[3] = { INVALID_BUFFER, ~0u, ~0u, gfx::DescriptorType::StorageBuffer };
}

void gfx::GBufferPass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	//Bind Pipeline
	const std::vector<RenderBatch>& renderBatches = renderer->mDrawBatches;
	if (renderBatches.size() == 0) return;

	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle materialBuffer = renderer->mMaterialBuffer;
	gfx::BufferHandle drawIndirectBuffer = renderer->mDrawIndirectBuffer;

	for (auto& batch : renderBatches)
	{
		if (batch.count == 0) continue;

		const gfx::BufferView& vbView = batch.vertexBuffer;
		const gfx::BufferView& ibView = batch.indexBuffer;

		descriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };

		descriptorInfos[2] = { transformBuffer, (uint32_t)(batch.offset * sizeof(glm::mat4)), (uint32_t)(batch.count * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };

		descriptorInfos[3] = { materialBuffer, (uint32_t)(batch.offset * sizeof(MaterialComponent)), (uint32_t)(batch.count * sizeof(MaterialComponent)), gfx::DescriptorType::StorageBuffer };

		device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));
		device->BindPipeline(commandList, pipeline);

		device->BindIndexBuffer(commandList, ibView.buffer);

		device->DrawIndexedIndirect(commandList, drawIndirectBuffer, batch.offset * sizeof(gfx::DrawIndirectCommand), batch.count, sizeof(gfx::DrawIndirectCommand));
	}
}
/*
void gfx::GBufferPass::OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height)
{
}
*/
void gfx::GBufferPass::Shutdown()
{
	gfx::GetDevice()->Destroy(pipeline);
}
