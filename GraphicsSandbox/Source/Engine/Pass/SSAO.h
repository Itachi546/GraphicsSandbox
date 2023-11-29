#pragma once

#include "../FrameGraph.h"
#include "../Graphics.h"

#include "../GlmIncludes.h"

class Renderer;

namespace gfx {

	class SSAO : public FrameGraphPass {

	public:
		SSAO(Renderer* device);

		void Initialize(RenderPassHandle renderPass) override;

		void Render(CommandList* commandList, Scene* scene) override;

		void AddUI() override;

		void Shutdown() override;

	private:
		BufferHandle kernelBuffer;
		TextureHandle randomRotationTexture;

		gfx::PipelineHandle pipeline;

		void GenerateKernelBuffer();
		void GenerateRandomTexture();

		struct PushConstants {
			glm::mat4 projectionMatrix;
			glm::mat4 normalViewMatrix;
			uint32_t depthTexture;
			uint32_t normalTexture;
			glm::vec2 noiseScale;
			int kernelSamples;
			float kernelRadius;
			float bias;
			float wsNormal;
		} pushConstants;

		const int RANDOM_TEXTURE_DIM = 4;

		gfx::DescriptorInfo descriptorInfos[2];
	};
}