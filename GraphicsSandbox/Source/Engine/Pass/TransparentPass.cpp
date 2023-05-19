#include "TransparentPass.h"

#include "../Renderer.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include "../Scene.h"
#include "../EnvironmentMap.h"
#include "../FrameGraph.h"
#include "../Camera.h"
#include "../GUI/ImGuiService.h"

gfx::TransparentPass::TransparentPass(RenderPassHandle renderPass, Renderer* renderer_) :
	renderer(renderer_)
{
	PipelineDesc pipelineDesc = {};

	ShaderDescription shaders[2];
	uint32_t size = 0;
	char* vertexCode = Utils::ReadFile(StringConstants::MAIN_VERT_PATH, &size);
	shaders[0] = { vertexCode, size };

	char* fragmentCode = Utils::ReadFile(StringConstants::MAIN_FRAG_PATH, &size);
	shaders[1] = { fragmentCode, size };

	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders;

	pipelineDesc.renderPass = renderPass;
	pipelineDesc.rasterizationState.enableDepthTest = false;
	pipelineDesc.rasterizationState.enableDepthWrite = false;
	pipelineDesc.rasterizationState.enableDepthClamp = true;

	gfx::BlendState blendState = {};
	blendState.enable = true;
	pipelineDesc.blendStates = &blendState;
	pipelineDesc.blendStateCount = 1;
	pipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);

	delete[] vertexCode;
	delete[] fragmentCode;
}

void gfx::TransparentPass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	DescriptorInfo descriptorInfo = { renderer->mLightBuffer, 0, sizeof(LightData) * 128, gfx::DescriptorType::StorageBuffer };
	device->UpdateDescriptor(pipeline, &descriptorInfo, 1);
	device->BindPipeline(commandList, pipeline);
	device->PushConstants(commandList, pipeline, gfx::ShaderStage::Fragment, &renderer->mEnvironmentData, sizeof(EnvironmentData), 0);
	device->Draw(commandList, 6, 0, 1);
}

void gfx::TransparentPass::Shutdown()
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	device->Destroy(pipeline);
}
