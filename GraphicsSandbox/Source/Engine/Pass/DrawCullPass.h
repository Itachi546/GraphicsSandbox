#pragma once

#include "../FrameGraph.h"

class Renderer;
class Scene;
struct RenderBatch;

namespace gfx {
	class DrawCullPass : public FrameGraphPass 
	{
	public:
		DrawCullPass(Renderer* renderer);
		
		void Initialize(RenderPassHandle renderPass) override;

		void Render(CommandList* commandList, Scene* scene) override;

		void Shutdown() override;

		Renderer* renderer;

		PipelineHandle pipeline;
		DescriptorInfo descriptorInfos[4];
	private:
		bool mSupportMeshShading = false;

	};
}