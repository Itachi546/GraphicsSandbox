#pragma once

#pragma once

#include "../FrameGraph.h"
#include "../GlmIncludes.h"

class Scene;
class Renderer;

namespace gfx {
	struct TransparentPass : public FrameGraphPass
	{
		TransparentPass(RenderPassHandle renderPass, Renderer* renderer);

		void Render(CommandList* commandList, Scene* scene) override;

		void Shutdown() override;

		PipelineHandle pipeline;
		Renderer* renderer;
	};
}

