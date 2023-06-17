#pragma once

#include "../FrameGraph.h"

class Scene;
class Renderer;
struct RenderBatch;

namespace gfx {

	struct DepthPrePass : public FrameGraphPass
	{
		DepthPrePass(Renderer* renderer);
		void Initialize(RenderPassHandle renderPass) override;
		void Render(CommandList* commandList, Scene* scene) override;
		void Shutdown() override;

		//void OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height);

		PipelineHandle indexedPipeline = INVALID_PIPELINE;
		DescriptorInfo indexedDescriptorInfos[3];

		PipelineHandle meshletPipeline = INVALID_PIPELINE;
		DescriptorInfo meshletDescriptorInfos[6];

		Renderer* renderer;

	private: 
		bool mSupportMeshShading = false;
		void drawIndexed(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches);
		void drawMeshlet(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches);
	};

}