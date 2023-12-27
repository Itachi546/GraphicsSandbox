#pragma once

#include "../FrameGraph.h"
#include "../Graphics.h"

class Renderer;
namespace gfx
{
	class BloomPass : public FrameGraphPass
	{
	public:
		//Bloom(gfx::GraphicsDevice* device, uint32_t width, uint32_t height, gfx::Format attachmentFormat = gfx::Format::R16B16G16A16_SFLOAT);
		BloomPass(Renderer* renderer, uint32_t width, uint32_t height);

		void Initialize(RenderPassHandle renderPass) override;

		void Render(CommandList* commandList, Scene* scene) override;

		void Shutdown() override;

	private:
		Renderer* mRenderer = nullptr;
		gfx::GraphicsDevice* mDevice = nullptr;

		gfx::TextureHandle mDownSampleTexture;
		gfx::TextureHandle mLightingTexture;

		gfx::PipelineHandle mDownSamplePipeline;
		gfx::PipelineHandle mUpSamplePipeline;
		gfx::PipelineHandle mCompositePipeline;

		uint32_t mWidth, mHeight;
		const uint32_t kMaxMipLevel = 5;
		const float mBlurRadius = 5.0f;
		const float mBloomStrength = 0.2f;

		void GenerateDownSamples(gfx::CommandList* commandList, gfx::TextureHandle brightTexture);
		void GenerateUpSamples(gfx::CommandList* commandList, float blurRadius);
		void Composite(gfx::CommandList* commandList);

	};
};