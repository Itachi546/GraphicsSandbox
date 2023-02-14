#pragma once

#include "Graphics.h"
#include "GlmIncludes.h"

#include <memory>

class Camera;

namespace gfx {
	class GraphicsDevice;
}
class CascadedShadowMap
{
public:
	CascadedShadowMap();

	void Update(Camera* camera, const glm::vec3& lightDirection);

	void BeginRender(gfx::CommandList* commandList);
	void EndRender(gfx::CommandList* commandList);

	void SetSplitLambda(float splitLambda) {
		this->kSplitLambda = splitLambda;
	}

	gfx::DescriptorInfo GetCascadeBufferDescriptor()
	{
		return gfx::DescriptorInfo{ mBuffer.get(), 0, sizeof(CascadeData), gfx::DescriptorType::UniformBuffer};
	}

	gfx::DescriptorInfo GetShadowMapDescriptor()
	{
		return gfx::DescriptorInfo{ &mFramebuffer->attachments[0], 0, 0, gfx::DescriptorType::Image};
	}

	void SetShadowDistance(float distance)
	{
		this->kShadowDistance = distance;
	}

	~CascadedShadowMap() = default;

private:
	std::unique_ptr<gfx::Pipeline> mPipeline;
	std::unique_ptr<gfx::Pipeline> mSkinnedPipeline;
	std::unique_ptr<gfx::RenderPass> mRenderPass;
	std::unique_ptr<gfx::Framebuffer> mFramebuffer;
	std::unique_ptr<gfx::GPUBuffer> mBuffer;

	const int kNumCascades = 5;
	const int kShadowDims = 4096;

	float kShadowDistance = 150.0f;
	float kSplitLambda = 0.85f;

	gfx::GraphicsDevice* mDevice;

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


	friend class Renderer;
};
