#include "TransparentPass.h"

#include "../Renderer.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include "../Scene.h"
#include "../EnvironmentMap.h"
#include "../FrameGraph.h"
#include "../Camera.h"

gfx::TransparentPass::TransparentPass(Renderer* renderer_) :
	renderer(renderer_)
{
}

void gfx::TransparentPass::Initialize(RenderPassHandle renderPass)
{
	ShaderPathInfo* pathInfo = ShaderPath::get("transparent_pass");

	PipelineDesc pipelineDesc = {};
	ShaderDescription shaders[2];
	uint32_t size = 0;
	char* vertexCode = Utils::ReadFile(pathInfo->shaders[0], &size);
	shaders[0] = { vertexCode, size };

	char* fragmentCode = Utils::ReadFile(pathInfo->shaders[1], &size);
	shaders[1] = { fragmentCode, size };

	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders;

	pipelineDesc.renderPass = renderPass;
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.enableDepthClamp = true;
	pipelineDesc.rasterizationState.cullMode = gfx::CullMode::None;

	gfx::BlendState blendState = {};
	blendState.enable = true;
	pipelineDesc.blendStates = &blendState;
	pipelineDesc.blendStateCount = 1;
	pipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);

	delete[] vertexCode;
	delete[] fragmentCode;

	descriptorInfos[0] = { renderer->mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
	descriptorInfos[6] = { renderer->mCascadeInfoBuffer, 0, sizeof(CascadeData), gfx::DescriptorType::UniformBuffer };
}

void gfx::TransparentPass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	std::vector<RenderBatch>& renderBatches = renderer->mTransparentBatches;
	if (renderBatches.size() == 0) return;
	drawIndexed(device, commandList, renderBatches);
}

void gfx::TransparentPass::drawIndexed(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches)
{
	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle materialBuffer = renderer->mMaterialBuffer;
	gfx::BufferHandle drawIndirectBuffer = renderer->mDrawIndirectBuffer;
	gfx::BufferHandle drawCommandCountBuffer = renderer->mDrawCommandCountBuffer;

	// Update push constant data
	mPushConstantData.brdfLUT = renderer->mEnvironmentData.brdfLUT;
	mPushConstantData.nLight = renderer->mEnvironmentData.nLight;
	mPushConstantData.irradianceMap = renderer->mEnvironmentData.irradianceMap;
	mPushConstantData.prefilterEnvMap = renderer->mEnvironmentData.prefilterEnvMap;
	mPushConstantData.cameraPosition = renderer->mEnvironmentData.cameraPosition;
	mPushConstantData.exposure = renderer->mEnvironmentData.exposure;
	mPushConstantData.globalAO = renderer->mEnvironmentData.globalAO;
	mPushConstantData.directionLightShadowMap = renderer->mEnvironmentData.directionalShadowMap;

	device->PushConstants(commandList, pipeline, ShaderStage::Fragment, &mPushConstantData, sizeof(mPushConstantData), 0);

	descriptorInfos[5] = { renderer->mLightBuffer, 0, sizeof(LightData) * 128, gfx::DescriptorType::StorageBuffer };

	for (const auto& batch : batches) {
		if (batch.count == 0) continue;

		const gfx::BufferView& vbView = batch.vertexBuffer;
		descriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };
		descriptorInfos[2] = { transformBuffer, (uint32_t)(batch.offset * sizeof(glm::mat4)), (uint32_t)(batch.count * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };
		descriptorInfos[3] = { drawIndirectBuffer, (uint32_t)(batch.offset * sizeof(MeshDrawIndirectCommand)), (uint32_t)(batch.count * sizeof(MeshDrawIndirectCommand)), gfx::DescriptorType::StorageBuffer };
		descriptorInfos[4] = { materialBuffer, (uint32_t)(batch.offset * sizeof(MaterialComponent)), (uint32_t)(batch.count * sizeof(MaterialComponent)), gfx::DescriptorType::StorageBuffer };

		const gfx::BufferView& ibView = batch.indexBuffer;

		device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));
		device->BindPipeline(commandList, pipeline);

		device->BindIndexBuffer(commandList, ibView.buffer);

		device->DrawIndexedIndirectCount(commandList,
			drawIndirectBuffer,
			batch.offset * sizeof(gfx::MeshDrawIndirectCommand),
			drawCommandCountBuffer,
			batch.id * sizeof(uint32_t),
			batch.count,
			sizeof(MeshDrawIndirectCommand));

	}
}

void gfx::TransparentPass::Shutdown()
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	device->Destroy(pipeline);
}
