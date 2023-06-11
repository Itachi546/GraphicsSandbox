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
	char* vertexCode = Utils::ReadFile(StringConstants::TRANSPARENT_VERT_PATH, &size);
	shaders[0] = { vertexCode, size };

	char* fragmentCode = Utils::ReadFile(StringConstants::MAIN_FRAG_PATH, &size);
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
	std::vector<DrawData>& drawDatas = scene->GetDrawDataTransparent();
	if (drawDatas.size() == 0) return;

	std::vector<TransparentDrawData> renderBatches;
	CreateBatch(scene, drawDatas, renderBatches);

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
	uint32_t lastOffset = (uint32_t)scene->GetDrawDataOpaque().size();
	for (auto& batch : renderBatches)
	{
		gfx::BufferView& vbView = batch.vb;
		gfx::BufferView& ibView = batch.ib;

		descriptorInfos[1] = { vbView.buffer, vbView.byteOffset, vbView.byteLength, gfx::DescriptorType::StorageBuffer};
		descriptorInfos[2] = { transformBuffer, sizeof(glm::mat4) * (batch.transformIndex + lastOffset), sizeof(glm::mat4), gfx::DescriptorType::StorageBuffer};
		descriptorInfos[3] = { materialBuffer, matSize * (batch.materialIndex + lastOffset), matSize, gfx::DescriptorType::StorageBuffer };

		device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));

		device->BindPipeline(commandList, pipeline);
		device->PushConstants(commandList, pipeline, ShaderStage::Fragment, &mPushConstantData, sizeof(PushConstantData), 0);

		device->BindIndexBuffer(commandList, ibView.buffer);
		device->DrawIndexed(commandList, batch.indexCount, 1, ibView.byteOffset / sizeof(uint32_t));
	}
}

void gfx::TransparentPass::Shutdown()
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	device->Destroy(pipeline);
}

void gfx::TransparentPass::CreateBatch(Scene* scene, std::vector<DrawData>& drawDatas, std::vector<TransparentDrawData>& batches)
{
	uint32_t count = 0;
	std::vector<glm::mat4> transforms;
	std::vector<MaterialComponent> materials;
	for (auto& drawData : drawDatas)
	{
		TransparentDrawData result;
		result.vb = drawData.vertexBuffer;
		result.ib = drawData.indexBuffer;
		result.indexCount = drawData.indexCount;

		result.materialIndex = count;
		result.transformIndex = count;

		transforms.push_back(std::move(drawData.worldTransform));
		materials.push_back(*drawData.material);
		count++;

		batches.push_back(std::move(result));
	}

	gfx::GraphicsDevice* device = gfx::GetDevice();

	uint32_t lastOffset = (uint32_t)scene->GetDrawDataOpaque().size();
	// @NOTE we cannot insert the new transform data to same buffer from start as it might 
    // be used by GPU. so we add the data to the last offset
	uint32_t transformCount = (uint32_t)transforms.size();
	device->CopyToBuffer(renderer->mTransformBuffer, transforms.data(), lastOffset * sizeof(glm::mat4), transformCount * sizeof(glm::mat4));

	uint32_t materialCount = (uint32_t)materials.size();
	device->CopyToBuffer(renderer->mMaterialBuffer, materials.data(), lastOffset * sizeof(MaterialComponent), materialCount * sizeof(MaterialComponent));

}
