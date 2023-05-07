#pragma once

#include "FrameGraph.h"

class Scene;
class Renderer;
namespace gfx {

	struct DepthPrePass : public FrameGraphRenderer
	{
		DepthPrePass(RenderPassHandle renderPass, Renderer* renderer);
		void render(CommandList* commandList, Scene* scene);

		PipelineHandle pipeline = INVALID_PIPELINE;
		DescriptorInfo descriptorInfos[3];
		Renderer* renderer;
	};

}