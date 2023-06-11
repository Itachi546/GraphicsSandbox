#pragma once

#include "../FrameGraph.h"
#include "../GlmIncludes.h"

class Scene;
class Renderer;

namespace gfx {
	struct LightingPass : public FrameGraphPass
	{
		LightingPass(Renderer* renderer);

		void Initialize(RenderPassHandle renderPass) override;

		void Render(CommandList* commandList, Scene* scene) override;

		void Shutdown() override;

		PipelineHandle pipeline;
		Renderer* renderer;

		
	};

}
