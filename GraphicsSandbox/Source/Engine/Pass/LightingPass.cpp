#include "LightingPass.h"

#include "../Renderer.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include "../Scene.h"
#include "../EnvironmentMap.h"
#include "../FrameGraph.h"
#include "../Camera.h"
#include "../GUI/ImGuiService.h"

gfx::LightingPass::LightingPass(RenderPassHandle renderPass, Renderer* renderer_) :
	renderer(renderer_)
{
	PipelineDesc pipelineDesc = {};

	ShaderDescription shaders[2];
	uint32_t size = 0;
	char* vertexCode = Utils::ReadFile(StringConstants::LIGHTING_VERT_PATH, &size);
	shaders[0] = { vertexCode, size };

	char* fragmentCode = Utils::ReadFile(StringConstants::LIGHTING_FRAG_PATH, &size);
	shaders[1] = { fragmentCode, size };

	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders;

	pipelineDesc.renderPass = renderPass;
	pipelineDesc.rasterizationState.enableDepthTest = false;
	pipelineDesc.rasterizationState.enableDepthWrite = false;
	pipelineDesc.rasterizationState.enableDepthClamp = true;

	gfx::BlendState blendState = {};
	pipelineDesc.blendStates = &blendState;
	pipelineDesc.blendStateCount = 1;
	pipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);

	delete[] vertexCode;
	delete[] fragmentCode;

	lightingInfo.exposure = 1.0f;
}

void gfx::LightingPass::AddUI()
{
	ImGui::SliderFloat("Exposure", &lightingInfo.exposure, 0.0f, 4.0f);
}

void gfx::LightingPass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	DescriptorInfo descriptorInfo = { renderer->mLightBuffer, 0, sizeof(LightData) * 128, gfx::DescriptorType::StorageBuffer };
	device->UpdateDescriptor(pipeline, &descriptorInfo, 1);
	device->BindPipeline(commandList, pipeline);

	auto& env = scene->GetEnvironmentMap();
	lightingInfo.brdfLUT = env->GetBRDFLUT().handle;
	lightingInfo.irradianceMap = env->GetIrradianceMap().handle;
	lightingInfo.prefilterEnvMap = env->GetPrefilterMap().handle;
	lightingInfo.nLight = (uint32_t)scene->GetComponentManager()->GetComponentArray<LightComponent>()->components.size();
	lightingInfo.pbrBuffer = renderer->mFrameGraphBuilder.AccessResource("gbuffer_metallic_roughness_occlusion")->info.texture.texture.handle;
	lightingInfo.colorBuffer = renderer->mFrameGraphBuilder.AccessResource("gbuffer_colour")->info.texture.texture.handle;
	lightingInfo.normalBuffer = renderer->mFrameGraphBuilder.AccessResource("gbuffer_normals")->info.texture.texture.handle;
	lightingInfo.positionBuffer = renderer->mFrameGraphBuilder.AccessResource("gbuffer_position")->info.texture.texture.handle;
	lightingInfo.cameraPosition = scene->GetCamera()->GetPosition();

	device->PushConstants(commandList, pipeline, gfx::ShaderStage::Fragment, &lightingInfo, sizeof(lightingInfo), 0);
	device->Draw(commandList, 6, 0, 1);
}

void gfx::LightingPass::Shutdown()
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	device->Destroy(pipeline);
}
