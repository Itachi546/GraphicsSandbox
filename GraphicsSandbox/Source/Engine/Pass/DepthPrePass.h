#pragma once

#include "../FrameGraph.h"

class Scene;
class Renderer;
namespace gfx {

	struct DepthPrePass : public FrameGraphPass
	{
		DepthPrePass(Renderer* renderer);
		void Initialize(RenderPassHandle renderPass) override;
		void Render(CommandList* commandList, Scene* scene) override;
		void Shutdown() override;

		//void OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height);

		PipelineHandle pipeline = INVALID_PIPELINE;
		DescriptorInfo descriptorInfos[3];
		Renderer* renderer;
	};

}