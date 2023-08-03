#pragma once

#include "../FrameGraph.h"
#include "../GlmIncludes.h"

#include <memory>

class Camera;
class Renderer;
class Scene;
struct RenderBatch;
namespace gfx {

	class GraphicsDevice;

	class CascadedShadowPass : public FrameGraphPass
	{
	public:
		CascadedShadowPass(Renderer* renderer);

		void Initialize(RenderPassHandle renderPass) override;

		void Render(CommandList* commandList, Scene* scene) override;

		void SetSplitLambda(float splitLambda) {
			this->kSplitLambda = splitLambda;
		}

		/*
		gfx::DescriptorInfo GetCascadeBufferDescriptor()
		{
			return gfx::DescriptorInfo{ mBuffer, 0, sizeof(CascadeData), gfx::DescriptorType::UniformBuffer};
		}
		*/

		void SetShadowDistance(float distance)
		{
			this->kShadowDistance = distance;
		}

		void Shutdown() override;
		virtual ~CascadedShadowPass() = default;

		DescriptorInfo descriptorInfos[4];

	private:
		gfx::PipelineHandle mPipeline = gfx::INVALID_PIPELINE;
		gfx::PipelineHandle mSkinnedPipeline = gfx::INVALID_PIPELINE;

		//gfx::RenderPassHandle mRenderPass = gfx::INVALID_RENDERPASS;
		//gfx::FramebufferHandle mFramebuffer = gfx::INVALID_FRAMEBUFFER;
		gfx::BufferHandle mCascadeInfoBuffer;

		//gfx::TextureHandle mAttachment0;

		const int kNumCascades = 5;
		const int kShadowDims = 4096;

		float kShadowDistance = 150.0f;
		float kSplitLambda = 0.85f;

		gfx::GraphicsDevice* mDevice;
		Renderer* renderer;

		struct Cascade {
			glm::mat4 VP;
			glm::vec4 splitDistance;
		};
		struct CascadeData
		{
			Cascade cascades[5];
			glm::vec4 shadowDims;
		} mCascadeData;

		void CalculateSplitDistance(Camera* camera);

		uint32_t colors[5] = {
		0xff0000,
		0xffff00,
		0xffffff,
		0x00ff00,
		0x0000ff
		};


		void update(Camera* camera, const glm::vec3& lightDirection);
		void render(CommandList* commandList);
	};
};