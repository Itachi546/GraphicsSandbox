#include "LightingPass.h"

#include "../Renderer.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include "../Scene.h"
#include "../EnvironmentMap.h"
#include "../FrameGraph.h"
#include "../Camera.h"
#include "../GUI/ImGuiService.h"

gfx::LightingPass::LightingPass(Renderer* renderer_) :
	renderer(renderer_)
{
}

void gfx::LightingPass::Initialize(RenderPassHandle renderPass)
{
	{
		ShaderPathInfo* shaderPathInfo = ShaderPath::get("lighting_pass");
		PipelineDesc pipelineDesc = {};
		ShaderDescription shaders[2];
		uint32_t size = 0;
		char* vertexCode = Utils::ReadFile(shaderPathInfo->shaders[0], &size);
		shaders[0] = { vertexCode, size };

		char* fragmentCode = Utils::ReadFile(shaderPathInfo->shaders[1], &size);
		shaders[1] = { fragmentCode, size };

		pipelineDesc.shaderCount = 2;
		pipelineDesc.shaderDesc = shaders;

		pipelineDesc.renderPass = renderPass;
		pipelineDesc.rasterizationState.enableDepthTest = false;
		pipelineDesc.rasterizationState.enableDepthWrite = false;
		pipelineDesc.rasterizationState.enableDepthClamp = true;
		pipelineDesc.rasterizationState.cullMode = gfx::CullMode::None;

		gfx::BlendState blendStates[2] = {};
		pipelineDesc.blendStates = blendStates;
		pipelineDesc.blendStateCount = 2;
		pipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);
		delete[] vertexCode;
		delete[] fragmentCode;
	}
	{
		ShaderPathInfo* shaderPathInfo = ShaderPath::get("cubemap");
		PipelineDesc pipelineDesc = {};
		ShaderDescription shaders[2];
		uint32_t size = 0;
		char* vertexCode = Utils::ReadFile(shaderPathInfo->shaders[0], &size);
		shaders[0] = { vertexCode, size };

		char* fragmentCode = Utils::ReadFile(shaderPathInfo->shaders[1], &size);
		shaders[1] = { fragmentCode, size };

		pipelineDesc.shaderCount = 2;
		pipelineDesc.shaderDesc = shaders;

		pipelineDesc.renderPass = renderPass;
		pipelineDesc.rasterizationState.enableDepthTest = true;
		pipelineDesc.rasterizationState.enableDepthWrite = false;
		pipelineDesc.rasterizationState.cullMode = gfx::CullMode::None;

		gfx::BlendState blendStates[2] = {};
		pipelineDesc.blendStates = blendStates;
		pipelineDesc.blendStateCount = 2;
		cubemapPipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);
		delete[] vertexCode;
		delete[] fragmentCode;
	}
}

void gfx::LightingPass::Render(CommandList* commandList, Scene* scene)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	DescriptorInfo descriptorInfo[] = { {renderer->mLightBuffer, 0, sizeof(LightData) * 128, gfx::DescriptorType::StorageBuffer},
		{renderer->mCascadeInfoBuffer, 0, sizeof(CascadeData), gfx::DescriptorType::UniformBuffer}
	};
	device->UpdateDescriptor(pipeline, descriptorInfo, (uint32_t)std::size(descriptorInfo));
	device->BindPipeline(commandList, pipeline);
	device->PushConstants(commandList, pipeline, gfx::ShaderStage::Fragment, &renderer->mEnvironmentData, sizeof(EnvironmentData), 0);
	device->Draw(commandList, 6, 0, 1);

	drawCubemap(commandList, scene);
}


void gfx::LightingPass::drawCubemap(gfx::CommandList* commandList, Scene* scene)
{
	// TODO: Define static Descriptor beforehand
	MeshRenderer* cubeRenderer = scene->GetComponentManager()->GetComponent<MeshRenderer>(scene->mCube);

	gfx::DescriptorInfo descriptorInfos[3] = {};
	descriptorInfos[0].buffer = renderer->mGlobalUniformBuffer;
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].size = sizeof(GlobalUniformData);
	descriptorInfos[0].type = gfx::DescriptorType::UniformBuffer;

	auto& vb = cubeRenderer->vertexBuffer;
	auto& ib = cubeRenderer->indexBuffer;

	descriptorInfos[1].buffer = vb.buffer;
	descriptorInfos[1].offset = vb.byteOffset;
	descriptorInfos[1].size = vb.byteLength;
	descriptorInfos[1].type = gfx::DescriptorType::StorageBuffer;

	std::unique_ptr<EnvironmentMap>& envMap = scene->GetEnvironmentMap();
	TextureHandle texture = envMap->GetCubemap();
	descriptorInfos[2].texture = &texture;
	descriptorInfos[2].offset = 0;
	descriptorInfos[2].type = gfx::DescriptorType::Image;


	gfx::GraphicsDevice* device = gfx::GetDevice();
	device->UpdateDescriptor(cubemapPipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	device->BindPipeline(commandList, cubemapPipeline);
	float bloomThreshold = renderer->mEnvironmentData.bloomThreshold;
	device->PushConstants(commandList, cubemapPipeline, ShaderStage::Fragment, &bloomThreshold, (uint32_t)sizeof(float));

	device->BindIndexBuffer(commandList, ib.buffer);
	device->DrawIndexed(commandList, cubeRenderer->GetIndexCount(), 1, ib.byteOffset / sizeof(uint32_t));
}


void gfx::LightingPass::Shutdown()
{
	gfx::GraphicsDevice* device = gfx::GetDevice();
	device->Destroy(pipeline);
	device->Destroy(cubemapPipeline);
}
