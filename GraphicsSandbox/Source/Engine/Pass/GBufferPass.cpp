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

	pipelineDesc.renderPass = renderPass;
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.enableDepthClamp = true;

	gfx::BlendState blendStates[5] = {};
	pipelineDesc.blendStates = blendStates;
	pipelineDesc.blendStateCount = 5;

	ShaderDescription shaders[3];
	pipelineDesc.shaderDesc = shaders;

	gfx::GraphicsDevice* device = gfx::GetDevice();
	mSupportMeshShading = device->SupportMeshShading();

	ShaderPathInfo* shaderPathInfo = ShaderPath::get("gbuffer_pass");
	if (mSupportMeshShading) {
		uint32_t size = 0;
		//char* taskCode = Utils::ReadFile(StringConstants::GBUFER_TASK_PATH, &size);
		//shaders[0] = { taskCode, size };

		char* meshCode = Utils::ReadFile(shaderPathInfo->meshShaders[1], &size);
		shaders[0] = { meshCode, size };

		char* fragmentCode = Utils::ReadFile(shaderPathInfo->shaders[1], &size);
		shaders[1] = { fragmentCode, size };

		pipelineDesc.shaderCount = 2;
		pipelineDesc.rasterizationState.cullMode = gfx::CullMode::Back;
		meshletPipeline = device->CreateGraphicsPipeline(&pipelineDesc);

		//delete[] taskCode;
		delete[] meshCode;
		delete[] fragmentCode;
		meshletDescriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
	}

	uint32_t size = 0;
	char* vertexCode = Utils::ReadFile(shaderPathInfo->shaders[0], &size);
	shaders[0] = { vertexCode, size };

	char* fragmentCode = Utils::ReadFile(shaderPathInfo->shaders[1], &size);
	shaders[1] = { fragmentCode, size };

	pipelineDesc.shaderCount = 2;
	indexedPipeline = device->CreateGraphicsPipeline(&pipelineDesc);

	delete[] vertexCode;
	delete[] fragmentCode;
	indexedDescriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };

}

void gfx::GBufferPass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	//Bind Pipeline
	const std::vector<RenderBatch>& renderBatches = renderer->mDrawBatches;
	if (renderBatches.size() == 0) return;

	if (mSupportMeshShading && renderer->mUseMeshShading)
		drawMeshlet(device, commandList, renderBatches);
	else
		drawIndexed(device, commandList, renderBatches);
}



void gfx::GBufferPass::drawIndexed(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches)
{
	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle materialBuffer = renderer->mMaterialBuffer;
	gfx::BufferHandle drawIndirectBuffer = renderer->mDrawIndirectBuffer;
	gfx::BufferHandle drawCommandCountBuffer = renderer->mDrawCommandCountBuffer;

	for (const auto& batch : batches) {
		if (batch.count == 0) continue;

		const gfx::BufferView& vbView = batch.vertexBuffer;
		indexedDescriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };
		indexedDescriptorInfos[2] = { transformBuffer, (uint32_t)(batch.offset * sizeof(glm::mat4)), (uint32_t)(batch.count * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };
		indexedDescriptorInfos[3] = { drawIndirectBuffer, (uint32_t)(batch.offset * sizeof(MeshDrawIndirectCommand)), (uint32_t)(batch.count * sizeof(MeshDrawIndirectCommand)), gfx::DescriptorType::StorageBuffer };
		indexedDescriptorInfos[4] = { materialBuffer, (uint32_t)(batch.offset * sizeof(MaterialComponent)), (uint32_t)(batch.count * sizeof(MaterialComponent)), gfx::DescriptorType::StorageBuffer };

		const gfx::BufferView& ibView = batch.indexBuffer;

		device->UpdateDescriptor(indexedPipeline, indexedDescriptorInfos, (uint32_t)std::size(indexedDescriptorInfos));
		device->BindPipeline(commandList, indexedPipeline);

		device->BindIndexBuffer(commandList, ibView.buffer);

		//device->DrawIndexedIndirect(commandList, drawIndirectBuffer, batch.offset * sizeof(gfx::DrawIndirectCommand), batch.count, sizeof(gfx::DrawIndirectCommand));
		device->DrawIndexedIndirectCount(commandList,
			drawIndirectBuffer,
			batch.offset * sizeof(gfx::MeshDrawIndirectCommand),
			drawCommandCountBuffer,
			batch.id * sizeof(uint32_t),
			batch.count,
			sizeof(MeshDrawIndirectCommand));
	}
}

void gfx::GBufferPass::drawMeshlet(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches)
{
	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle materialBuffer = renderer->mMaterialBuffer;
	gfx::BufferHandle dib = renderer->mDrawIndirectBuffer;
	gfx::BufferHandle dcb = renderer->mDrawCommandCountBuffer;

	for (const auto& batch : batches) {
		const gfx::BufferView& vbView = batch.vertexBuffer;
		meshletDescriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[2] = { transformBuffer, (uint32_t)(batch.offset * sizeof(glm::mat4)), (uint32_t)(batch.count * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[3] = { dib, (uint32_t)(batch.offset * sizeof(MeshDrawIndirectCommand)), (uint32_t)(batch.count * sizeof(MeshDrawIndirectCommand)), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[4] = { materialBuffer, (uint32_t)(batch.offset * sizeof(MaterialComponent)), (uint32_t)(batch.count * sizeof(MaterialComponent)), gfx::DescriptorType::StorageBuffer };
	
		meshletDescriptorInfos[5] = { batch.meshletBuffer, 0, device->GetBufferSize(batch.meshletBuffer), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[6] = { batch.meshletVertexBuffer, 0, device->GetBufferSize(batch.meshletVertexBuffer), gfx::DescriptorType::StorageBuffer };
		meshletDescriptorInfos[7] = { batch.meshletTriangleBuffer, 0, device->GetBufferSize(batch.meshletTriangleBuffer), gfx::DescriptorType::StorageBuffer };

		device->UpdateDescriptor(meshletPipeline, meshletDescriptorInfos, (uint32_t)std::size(meshletDescriptorInfos));
		device->BindPipeline(commandList, meshletPipeline);

		uint32_t stride = sizeof(MeshDrawIndirectCommand);
		uint32_t offset = batch.offset * sizeof(gfx::MeshDrawIndirectCommand) + offsetof(gfx::MeshDrawIndirectCommand, taskCount);

		device->DrawMeshTasksIndirectCount(commandList, dib, offset, dcb, batch.id * sizeof(uint32_t),
			batch.count,
			stride);
	}
}

void gfx::GBufferPass::Shutdown()
{
	if (indexedPipeline.handle != gfx::K_INVALID_RESOURCE_HANDLE)
		gfx::GetDevice()->Destroy(indexedPipeline);
	if (meshletPipeline.handle != gfx::K_INVALID_RESOURCE_HANDLE)
		gfx::GetDevice()->Destroy(meshletPipeline);
}

