#include "FXAAPass.h"
#include "../StringConstants.h"
#include "../Utils.h"
#include "../Renderer.h"
#include "../GUI/ImGuiService.h"

gfx::FXAAPass::FXAAPass(Renderer* renderer, uint32_t width, uint32_t height) : renderer(renderer)
{
	uniformData.invHeight = 1.0f / width;
	uniformData.invHeight = 1.0f / height;
	uniformData.edgeThresholdMax = 0.0312f;
	uniformData.edgeThresholdMin = 0.125f;
}

void gfx::FXAAPass::Initialize(RenderPassHandle renderPass)
{
	PipelineDesc pipelineDesc = {};

	ShaderDescription shaders[2];
	uint32_t size = 0;
	char* vertexCode = Utils::ReadFile(StringConstants::FULLSCREEN_VERT_PATH, &size);
	shaders[0] = { vertexCode, size };

	char* fragmentCode = Utils::ReadFile(StringConstants::FXAA_FRAG_PATH, &size);
	shaders[1] = { fragmentCode, size };

	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders;

	pipelineDesc.renderPass = renderPass;
	pipelineDesc.rasterizationState.enableDepthTest = false;
	pipelineDesc.rasterizationState.enableDepthWrite = false;

	gfx::BlendState blendState = {};
	pipelineDesc.blendStates = &blendState;
	pipelineDesc.blendStateCount = 1;
	pipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);

	delete[] vertexCode;
	delete[] fragmentCode;
}

void gfx::FXAAPass::Render(CommandList* commandList, Scene* scene)
{
	// Update textureId, probably not needed to do it every frame
	uniformData.inputTextureHandle = renderer->mFrameGraphBuilder.AccessResource("lighting")->info.texture.texture.handle;

	gfx::GraphicsDevice* device = gfx::GetDevice();
	device->BindPipeline(commandList, pipeline);
	device->PushConstants(commandList, pipeline, gfx::ShaderStage::Fragment, &uniformData, sizeof(uniformData));
	device->Draw(commandList, 6, 0, 1);
}

void gfx::FXAAPass::Shutdown()
{
	gfx::GetDevice()->Destroy(pipeline);
}

void gfx::FXAAPass::AddUI()
{
	/*
	if (ImGui::CollapsingHeader("FXAA")) {
		ImGui::SliderFloat("edgeMinThreshold", &uniformData.edgeThresholdMin, 0.0f, 0.4f);
		ImGui::SliderFloat("edgeMaxThreshold", &uniformData.edgeThresholdMax, 0.0f, 0.4f);
	}
	*/
}

void gfx::FXAAPass::OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height)
{
	if (width > 0 && height > 0)
	{
		uniformData.invWidth = 1.0f / 1920.0f;
		uniformData.invHeight = 1.0f / 1080.0f;
	}
}
