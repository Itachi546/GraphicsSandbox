#pragma once

#include "../FrameGraph.h"
#include "../Graphics.h"

class Renderer;
namespace gfx {
	struct GBufferPass : public FrameGraphPass
	{

		GBufferPass(RenderPassHandle renderPass, Renderer* renderer);

		void Render(CommandList* commandList, Scene* scene);

		void Shutdown();

		gfx::PipelineHandle pipeline;
		Renderer* renderer;

		DescriptorInfo descriptorInfos[4];
	};
}