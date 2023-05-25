#pragma once

#include "../FrameGraph.h"

class Renderer;
class Scene;

namespace gfx {

	class FXAAPass : public FrameGraphPass
	{
	public:
		FXAAPass(RenderPassHandle renderPass, Renderer* renderer, uint32_t width, uint32_t height);

		void Render(CommandList* commandList, Scene* scene) override;
		void Shutdown() override;

		void AddUI() override;

		void OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height);

		PipelineHandle pipeline = INVALID_PIPELINE;
		DescriptorInfo descriptorInfo;
		Renderer* renderer;

	private:
		struct UniformData {
			uint32_t inputTextureHandle;
			float invWidth;
			float invHeight;
			float edgeThresholdMin;
			float edgeThresholdMax;
		} uniformData;
	};
}