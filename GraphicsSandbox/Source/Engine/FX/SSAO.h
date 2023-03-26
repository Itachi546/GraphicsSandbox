#include "../Graphics.h"
#include "../GraphicsDevice.h"
#include "../GlmIncludes.h"

#include <array>

namespace fx
{
	class SSAO
	{
	public:
		SSAO(gfx::GraphicsDevice* device, uint32_t width, uint32_t height);

		void SetSize(uint32_t width, uint32_t height) {
			mWidth = width;
			mHeight = height;
		}

		void Generate(gfx::CommandList* commandList, gfx::TextureHandle depthTexture, float blurRadius);

	private:
		uint32_t mWidth, mHeight;
		gfx::GraphicsDevice* mDevice;
		gfx::PipelineHandle mPipeline;
		gfx::TextureHandle mTexture;
		gfx::TextureHandle mNoiseTexture;
		gfx::BufferHandle mKernelBuffer;

		void Initialize();
	};
}