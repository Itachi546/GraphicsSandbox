#pragma once

#include "../FrameGraph.h"
#include "../GlmIncludes.h"
#include "../Renderer.h"

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

		void AddUI() override;

		void SetShadowDims(uint32_t width, uint32_t height) {
			mShadowDims = glm::vec2(width, height);
		}

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

		const int kNumCascades = 5;

		glm::vec2 mShadowDims = glm::vec2(2048.0f, 2048.0f);
		float kShadowDistance = 80.0f;
		float kSplitLambda = 0.9f;

		gfx::GraphicsDevice* mDevice;
		Renderer* renderer;

		CascadeData mCascadeData;

		void CalculateSplitDistance(Camera* camera);

		uint32_t colors[5] = {
		0xff000022,
		0xffff0022,
		0xffffff22,
		0x00ff0022,
		0x0000ff22
		};

		glm::mat4 cameraVP = glm::mat4(1.0f);
		bool mFreezeCameraFrustum = false;
		bool mEnableCascadeDebug = false;
		glm::vec3* cameraFrustumPoints = nullptr;
		int debugCascadeIndex = 0;

		TextureHandle csmTexture = gfx::INVALID_TEXTURE;

		void update(Camera* camera, const glm::vec3& lightDirection);
		void render(CommandList* commandList);
	};
};