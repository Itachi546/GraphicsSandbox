#pragma once

#include "../FrameGraph.h"

class Scene;
class Renderer;

namespace gfx {

	class BlurPass : public FrameGraphPass {

	public:
		BlurPass(Renderer* renderer, uint32_t width, uint32_t height);

		void Initialize(RenderPassHandle renderPass) override;

		void SetInputTexture(TextureHandle texture) {
			inputTexture = texture;
		}

		void SetOutputTexture(TextureHandle texture) {
			outputTexture = texture;
		}

		void Render(CommandList* commandList, Scene* scene) override;

		void Shutdown() override;

	private:
		PipelineHandle pipeline;
		DescriptorInfo descriptorInfos[2];

		uint32_t width, height;

		TextureHandle inputTexture = gfx::INVALID_TEXTURE;
		TextureHandle outputTexture = gfx::INVALID_TEXTURE;
	};

}
