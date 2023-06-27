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

		void AddUI() override;

		Renderer* renderer;

		PipelineHandle pipeline;
		DescriptorInfo descriptorInfos[5];
	private:
		uint32_t totalVisibleMesh = 0;
		uint32_t totalMesh = 0;
		bool mSupportMeshShading = false;
		BufferHandle visibleMeshCountBuffer;
	};
}