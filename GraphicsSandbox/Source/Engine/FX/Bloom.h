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

		void Generate(gfx::CommandList* commandList, gfx::GPUTexture* inputTexture);

		gfx::GPUTexture* GetTexture() { return &mDownSampleTexture; }

	private:
		gfx::Format mFormat;

		gfx::GraphicsDevice* mDevice;

		gfx::GPUTexture mDownSampleTexture;

		std::shared_ptr<gfx::Pipeline> mDownSamplePipeline;
		std::shared_ptr<gfx::Pipeline> mUpSamplePipeline;

		uint32_t mWidth, mHeight;
		const uint32_t kMaxMipLevel = 5;

		void Initialize();
	};
};