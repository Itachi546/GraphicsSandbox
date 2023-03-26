#pragma once

#include "../Graphics.h"
#include "../GraphicsDevice.h"

namespace fx
{
	class Bloom
	{
	public:
		Bloom(gfx::GraphicsDevice* device, uint32_t width, uint32_t height, gfx::Format attachmentFormat = gfx::Format::R16B16G16A16_SFLOAT);

		void SetSize(uint32_t width, uint32_t height) {
			mWidth = width;
			mHeight = height;
		}

		// Generate Bloom texture from brightTexture
		void Generate(gfx::CommandList* commandList, gfx::GPUTexture* brightTexture, float blurRadius);
		// Composite the Bloom texture on renderTarget
		void Composite(gfx::CommandList* commandList, gfx::GPUTexture* hdrTexture, float bloomStrength);
		gfx::GPUTexture* GetTexture() { return &mDownSampleTexture; }

	private:

		gfx::Format mFormat;

		gfx::GraphicsDevice* mDevice;

		gfx::GPUTexture mDownSampleTexture;

		gfx::PipelineHandle mDownSamplePipeline;
		gfx::PipelineHandle mUpSamplePipeline;
		gfx::PipelineHandle mCompositePipeline;

		uint32_t mWidth, mHeight;
		const uint32_t kMaxMipLevel = 5;

		void Initialize();

		void GenerateDownSamples(gfx::CommandList* commandList, gfx::GPUTexture* brightTexture);
		void GenerateUpSamples(gfx::CommandList* commandList, float blurRadius);
	};
};