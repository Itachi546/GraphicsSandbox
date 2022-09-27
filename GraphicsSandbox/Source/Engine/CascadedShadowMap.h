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

	~CascadedShadowMap() = default;

private:
	std::unique_ptr<gfx::Pipeline> mPipeline;
	std::unique_ptr<gfx::RenderPass> mRenderPass;
	std::unique_ptr<gfx::Framebuffer> mFramebuffer;
	std::unique_ptr<gfx::GPUBuffer> mBuffer;

	const int kNumCascades = 5;
	const int kShadowDims = 512;
	const float kShadowDistance = 150.0f;
	float mSplitDistance[5] = {};
	float kSplitLambda = 0.85f;
	float kNearDistance = 0.01f;

	gfx::GraphicsDevice* mDevice;

	struct CascadeData
	{
		int nCascade;
		glm::mat4 lightVP[5];
	} mCascadeData;

	void CalculateSplitDistance();
};
