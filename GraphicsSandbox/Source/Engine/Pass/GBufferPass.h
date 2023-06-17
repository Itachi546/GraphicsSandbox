#pragma once

#include "../FrameGraph.h"
#include "../Graphics.h"

class Renderer;
struct RenderBatch;

namespace gfx {
	struct GBufferPass : public FrameGraphPass
	{

		GBufferPass(Renderer* renderer);

		void Initialize(RenderPassHandle renderPass) override;

		void Render(CommandList* commandList, Scene* scene) override;

		//void OnResize(gfx::GraphicsDevice* device, uint32_t width, uint32_t height) override;

		void Shutdown() override;

		gfx::PipelineHandle indexedPipeline = gfx::INVALID_PIPELINE;
		DescriptorInfo indexedDescriptorInfos[4];

		// Mesh shader pipeline
		gfx::PipelineHandle meshletPipeline = gfx::INVALID_PIPELINE;
		DescriptorInfo meshletDescriptorInfos[7];

		Renderer* renderer;

	private:
		void drawIndexed(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches);
		void drawMeshlet(gfx::GraphicsDevice* device, gfx::CommandList* commandList, const std::vector<RenderBatch>& batches);
		bool mSupportMeshShading = false;
	};
}