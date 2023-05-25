#pragma once

#include "../FrameGraph.h"
#include "../Graphics.h"

class Renderer;
namespace gfx {
	struct GBufferPass : public FrameGraphPass
	{

		GBufferPass(Renderer* renderer);

		void Initialize(RenderPassHandle renderPass) override;

		void Render(CommandList* commandList, Scene* scene) override;

		//void OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height) override;

		void Shutdown() override;

		gfx::PipelineHandle pipeline;
		Renderer* renderer;

		DescriptorInfo descriptorInfos[4];
	};
}