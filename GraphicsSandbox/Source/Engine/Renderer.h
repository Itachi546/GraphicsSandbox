#pragma once

#include "GlmIncludes.h"
#include "Graphics.h"
#include "GraphicsDevice.h"

#include <vector>

class Scene;

class Renderer
{
public:
	Renderer(std::shared_ptr<gfx::RenderPass> swapchainRP);

	void Update(float dt, int width, int height);

	void SetScene(Scene* scene) { mScene = scene; }

	void Render();

	~Renderer() = default;

private:
	struct GlobalUniformData
	{
		glm::mat4 P;
		glm::mat4 V;
		glm::mat4 VP;
		glm::vec3 cameraPosition;
		float dt;
	} mGlobalUniformData;

	gfx::GraphicsDevice* mDevice;
	std::shared_ptr<gfx::RenderPass> mSwapchainRP;
	Scene* mScene;

	std::shared_ptr<gfx::Pipeline> mTrianglePipeline;
	std::shared_ptr<gfx::Pipeline> mCubemapPipeline;


	std::shared_ptr<gfx::GPUBuffer> mGlobalUniformBuffer;
	std::shared_ptr<gfx::GPUBuffer> mTransformBuffer;
	std::shared_ptr<gfx::GPUBuffer> mPerObjectDataBuffer;
	std::shared_ptr<gfx::GPUBuffer> mDrawIndirectBuffer;
	gfx::Semaphore mAcquireSemaphore;
	gfx::Semaphore mReleaseSemaphore;

	std::vector<gfx::DescriptorInfo> mDescriptorInfos;
	const int kMaxEntity = 10'000;

	std::shared_ptr<gfx::Pipeline> loadPipelines(const char* vsPath, const char* fsPath, gfx::CullMode cullMode = gfx::CullMode::Back);
	void DrawCubemap(gfx::CommandList* commandList, gfx::GPUTexture* cubemap);
};
