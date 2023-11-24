#include "SSAO.h"

#include "../StringConstants.h"
#include "../Utils.h"
#include "../Renderer.h"
#include "../Scene.h"
#include "../Camera.h"
#include "../TextureCache.h"

#include "../GUI/ImGuiService.h"
#include <random>

namespace gfx {

	SSAO::SSAO(Renderer* renderer)
	{
		pushConstants.depthTexture = renderer->mFrameGraphBuilder.AccessResource("depth")->info.texture.texture.handle;
		pushConstants.normalTexture = renderer->mFrameGraphBuilder.AccessResource("gbuffer_normals")->info.texture.texture.handle;

		pushConstants.kernelRadius = 0.5f;
		pushConstants.bias = 0.025f;
		pushConstants.kernelSamples = 64;
		pushConstants.noiseScale = glm::vec2(960.0f / 4.0f, 540.0f / 4.0f);
	}

	void SSAO::Initialize(RenderPassHandle renderPass)
	{
		ShaderPathInfo* shaderPathInfo = ShaderPath::get("ssao_pass");
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

		gfx::BlendState blendState = {};
		pipelineDesc.blendStates = &blendState;
		pipelineDesc.blendStateCount = 1;
		pipeline = gfx::GetDevice()->CreateGraphicsPipeline(&pipelineDesc);

		delete[] vertexCode;
		delete[] fragmentCode;

		GenerateKernelBuffer();
		GenerateRandomTexture();

		descriptorInfos[0] = DescriptorInfo{ kernelBuffer, 0, static_cast<uint32_t>(sizeof(glm::vec4) * pushConstants.kernelSamples), DescriptorType::UniformBuffer };
		descriptorInfos[1] = DescriptorInfo{ &randomRotationTexture, 0, 0, DescriptorType::Image };
	}

	void SSAO::Render(CommandList* commandList, Scene* scene)
	{
		pushConstants.projection = scene->GetCamera()->GetProjectionMatrix();
		gfx::GraphicsDevice* device = gfx::GetDevice();

		device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));

		device->BindPipeline(commandList, pipeline);
		device->PushConstants(commandList, pipeline, gfx::ShaderStage::Fragment, &pushConstants, sizeof(PushConstants));


		device->Draw(commandList, 6, 0, 1);
	}

	void SSAO::AddUI()
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("SSAO")) {
			ImGui::SliderFloat("kernelRadius", &pushConstants.kernelRadius, 0.0f, 2.0f);
			ImGui::SliderInt("kernelSamples", &pushConstants.kernelSamples, 0, 64);
			ImGui::SliderFloat("bias", &pushConstants.bias, 0.0f, 0.1f);
		}
	}

	void SSAO::Shutdown()
	{
		gfx::GraphicsDevice* device = gfx::GetDevice();
		device->Destroy(pipeline);
		device->Destroy(kernelBuffer);
		device->Destroy(randomRotationTexture);
	}

	void SSAO::GenerateKernelBuffer()
	{
		std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
		std::default_random_engine generator;
		std::vector<glm::vec4> ssaoKernel;

		for (uint32_t i = 0; i < uint32_t(pushConstants.kernelSamples); ++i) {
			glm::vec3 sample(randomFloats(generator) * 2.0f - 1.0f,
				randomFloats(generator) * 2.0f - 1.0f,
				randomFloats(generator));
			sample = glm::normalize(sample);
			sample *= randomFloats(generator);

			float scale = float(i) / 64.0f;
			scale = glm::lerp(0.1f, 1.0f, scale * scale);
			sample *= scale;
			ssaoKernel.push_back(glm::vec4(sample, 1.0f));
		}

		gfx::GraphicsDevice* device = gfx::GetDevice();

		uint32_t dataSize = (uint32_t)(ssaoKernel.size() * sizeof(glm::vec4));

		gfx::GPUBufferDesc bufferDesc = {};
		bufferDesc.bindFlag = gfx::BindFlag::ConstantBuffer;
		bufferDesc.size = dataSize;
		kernelBuffer = device->CreateBuffer(&bufferDesc);
		device->CopyToBuffer(kernelBuffer, ssaoKernel.data(), 0, dataSize);
	}

	void SSAO::GenerateRandomTexture()
	{
		std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
		std::default_random_engine generator;

		std::vector<glm::vec2> rotationVector;
		for (uint32_t i = 0; i < 16; ++i) {
			rotationVector.emplace_back(glm::vec2(
				randomFloats(generator) * 2.0f - 1.0f,
				randomFloats(generator) * 2.0f - 1.0f
			));
		}
		
		gfx::GPUTextureDesc textureDesc = {};
		textureDesc.bCreateSampler = true;
		textureDesc.width = 4;
		textureDesc.height = 4;
		textureDesc.bindFlag = gfx::BindFlag::ShaderResource;
		textureDesc.format = gfx::Format::R32G32_SFLOAT;
		textureDesc.bAddToBindless = false;
		textureDesc.samplerInfo.wrapMode = TextureWrapMode::Repeat;
		textureDesc.samplerInfo.textureFilter = TextureFilter::Nearest;

		gfx::GraphicsDevice* device = gfx::GetDevice();
		randomRotationTexture = device->CreateTexture(&textureDesc);

		const uint32_t imageDataSize = static_cast<uint32_t>(rotationVector.size() * sizeof(glm::vec2));
		device->CopyTexture(randomRotationTexture, rotationVector.data(), imageDataSize, 0, 0, true);
	}

};