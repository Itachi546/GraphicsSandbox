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
	gfx::GraphicsDevice* device = gfx::GetDevice();
	mSupportMeshShading = device->SupportMeshShading();

	PipelineDesc pipelineDesc = {};
	pipelineDesc.renderPass = renderPass;
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.enableDepthClamp = true;

	if (mSupportMeshShading) {
		uint32_t size = 0;
		char* code = Utils::ReadFile(StringConstants::DEPTH_PREPASS_MESH_PATH, &size);
		ShaderDescription shader = { code, size };
		pipelineDesc.shaderCount = 1;
		pipelineDesc.shaderDesc = &shader;
		meshletPipeline = device->CreateGraphicsPipeline(&pipelineDesc);
		meshletDescriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
		delete[] code;
	}
	
	uint32_t size = 0;
	char* code = Utils::ReadFile(StringConstants::DEPTH_PREPASS_PATH, &size);
	ShaderDescription shader = { code, size };
	pipelineDesc.shaderCount = 1;
	pipelineDesc.shaderDesc = &shader;
	indexedPipeline = device->CreateGraphicsPipeline(&pipelineDesc);
	indexedDescriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
	delete[] code;
}

void gfx::DepthPrePass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	//Bind Pipeline
	const std::vector<RenderBatch>& renderBatches = renderer->mDrawBatches;

	if (mSupportMeshShading && renderer->mUseMeshShading)
		drawMeshlet(device, commandList, renderBatches);
	else
		drawIndexed(device, commandList, renderBatches);
}

void gfx::DepthPrePass::drawIndexed(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches)
{
	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle drawIndirectBuffer = renderer->mDrawIndirectBuffer;
	gfx::BufferHandle drawCommandCountBuffer = renderer->mDrawCommandCountBuffer;

	uint32_t batchCount = 0;
	for (const auto& batch : batches) {
		if (batch.count == 0) continue;

		const gfx::BufferView& vbView = batch.vertexBuffer;
		indexedDescriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };
		indexedDescriptorInfos[2] = { transformBuffer, (uint32_t)(batch.offset * sizeof(glm::mat4)), (uint32_t)(batch.count * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };
		indexedDescriptorInfos[3] = { drawIndirectBuffer, (uint32_t)(batch.offset * sizeof(MeshDrawIndirectCommand)), (uint32_t)(batch.count * sizeof(MeshDrawIndirectCommand)), gfx::DescriptorType::StorageBuffer };

		const gfx::BufferView& ibView = batch.indexBuffer;

		device->UpdateDescriptor(indexedPipeline, indexedDescriptorInfos, (uint32_t)std::size(indexedDescriptorInfos));
		device->BindPipeline(commandList, indexedPipeline);

		device->BindIndexBuffer(commandList, ibView.buffer);

		//device->DrawIndexedIndirect(commandList, drawIndirectBuffer, batch.offset * sizeof(gfx::MeshDrawIndirectCommand), batch.count, sizeof(gfx::MeshDrawIndirectCommand));
		device->DrawIndexedIndirectCount(commandList,
			drawIndirectBuffer,
			batch.offset * sizeof(gfx::MeshDrawIndirectCommand),
			drawCommandCountBuffer,
			batch.id * sizeof(uint32_t),
			batch.count,
			sizeof(MeshDrawIndirectCommand));
	}
}

void gfx::DepthPrePass::drawMeshlet(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches)
{
	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle dib = renderer->mDrawIndirectBuffer;
	gfx::BufferHandle dcb = renderer->mDrawCommandCountBuffer;

	for (const auto& batch : batches) {
		const gfx::BufferView& vbView = batch.vertexBuffer;
		meshletDescriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[2] = { transformBuffer, (uint32_t)(batch.offset * sizeof(glm::mat4)), (uint32_t)(batch.count * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[3] = { dib, (uint32_t)(batch.offset * sizeof(MeshDrawIndirectCommand)), (uint32_t)(batch.count * sizeof(MeshDrawIndirectCommand)), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[4] = { batch.meshletBuffer, 0, device->GetBufferSize(batch.meshletBuffer), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[5] = { batch.meshletVertexBuffer, 0, device->GetBufferSize(batch.meshletVertexBuffer), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[6] = { batch.meshletTriangleBuffer, 0, device->GetBufferSize(batch.meshletTriangleBuffer), gfx::DescriptorType::StorageBuffer };

		device->UpdateDescriptor(meshletPipeline, meshletDescriptorInfos, (uint32_t)std::size(meshletDescriptorInfos));
		device->BindPipeline(commandList, meshletPipeline);

		uint32_t stride = sizeof(MeshDrawIndirectCommand);
		uint32_t offset = batch.offset * sizeof(gfx::MeshDrawIndirectCommand) + offsetof(gfx::MeshDrawIndirectCommand, taskCount);
		device->DrawMeshTasksIndirectCount(commandList, dib, offset, dcb, batch.id * sizeof(uint32_t),
			batch.count,
			stride);
	}
}


void gfx::DepthPrePass::Shutdown()
{
	if(indexedPipeline.handle != gfx::K_INVALID_RESOURCE_HANDLE)
		gfx::GetDevice()->Destroy(indexedPipeline);
	if (meshletPipeline.handle != gfx::K_INVALID_RESOURCE_HANDLE)
		gfx::GetDevice()->Destroy(meshletPipeline);
}

/*
void gfx::DepthPrePass::OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height)
{
}
*/