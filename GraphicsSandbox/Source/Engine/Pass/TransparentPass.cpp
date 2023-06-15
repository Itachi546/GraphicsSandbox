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
	PipelineDesc pipelineDesc = {};

	ShaderDescription shaders[2];
	uint32_t size = 0;
	char* vertexCode = Utils::ReadFile(StringConstants::MAIN_VERT_PATH, &size);
	shaders[0] = { vertexCode, size };

	char* fragmentCode = Utils::ReadFile(StringConstants::TRANSPARENT_FRAG_PATH, &size);
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
}

void gfx::TransparentPass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	std::vector<RenderBatch>& renderBatches = renderer->mTransparentBatches;
	if (renderBatches.size() == 0) return;

	// Draw Batch
	gfx::BufferHandle transformBuffer = renderer->mTransformBuffer;
	gfx::BufferHandle drawIndirectBuffer = renderer->mDrawIndirectBuffer;
	gfx::BufferHandle materialBuffer = renderer->mMaterialBuffer;

	uint32_t count = (uint32_t)renderBatches.size();

	descriptorInfos[4] = { renderer->mLightBuffer, 0, sizeof(LightData) * 128, gfx::DescriptorType::StorageBuffer };

	// Update push constant data
	mPushConstantData.brdfLUT = renderer->mEnvironmentData.brdfLUT;
	mPushConstantData.nLight = renderer->mEnvironmentData.nLight;
	mPushConstantData.irradianceMap = renderer->mEnvironmentData.irradianceMap;
	mPushConstantData.prefilterEnvMap = renderer->mEnvironmentData.prefilterEnvMap;
	mPushConstantData.cameraPosition = renderer->mEnvironmentData.cameraPosition;
	mPushConstantData.exposure = renderer->mEnvironmentData.exposure;
	mPushConstantData.globalAO = renderer->mEnvironmentData.globalAO;

	uint32_t matSize = sizeof(MaterialComponent);
	for (auto& batch : renderBatches)
	{
		if (batch.count == 0) continue;

		const gfx::BufferView& vbView = batch.vertexBuffer;
		const gfx::BufferView& ibView = batch.indexBuffer;

		descriptorInfos[1] = { vbView.buffer, 0, device->GetBufferSize(vbView.buffer), gfx::DescriptorType::StorageBuffer };

		descriptorInfos[2] = { transformBuffer, (uint32_t)(batch.offset * sizeof(glm::mat4)), (uint32_t)(batch.count * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer };

		descriptorInfos[3] = { materialBuffer, (uint32_t)(batch.offset * sizeof(MaterialComponent)), (uint32_t)(batch.count * sizeof(MaterialComponent)), gfx::DescriptorType::StorageBuffer };

		device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));
		device->PushConstants(commandList, pipeline, ShaderStage::Fragment, &mPushConstantData, sizeof(mPushConstantData), 0);
		device->BindPipeline(commandList, pipeline);

		device->BindIndexBuffer(commandList, ibView.buffer);

		device->DrawIndexedIndirect(commandList, drawIndirectBuffer, batch.offset * sizeof(gfx::DrawIndirectCommand), batch.count, sizeof(gfx::DrawIndirectCommand));

	}
}

void gfx::TransparentPass::Shutdown()
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	device->Destroy(pipeline);
}
