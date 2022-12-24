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

		void Generate(gfx::CommandList* commandList, gfx::GPUTexture* depthTexture, float blurRadius);

	private:
		uint32_t mWidth, mHeight;
		gfx::GraphicsDevice* mDevice;
		std::shared_ptr<gfx::Pipeline> mPipeline;
		std::shared_ptr<gfx::GPUTexture> mTexture;
		std::shared_ptr<gfx::GPUTexture> mNoiseTexture;
		std::shared_ptr <gfx::GPUBuffer> mKernelBuffer;

		void Initialize();
	};
}