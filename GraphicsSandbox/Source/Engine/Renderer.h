#pragma once

#include "GlmIncludes.h"
#include "Graphics.h"
#include "GraphicsDevice.h"

#include <vector>

class Scene;

class Renderer
{
public:
	Renderer();

	void Update(float dt);

	void SetScene(Scene* scene) { mScene = scene; }

	void Render();

	~Renderer() = default;

private:
	struct LightData
	{
		glm::vec3 position;
		float radius;
		glm::vec3 color;
		float lightType;
	};
	struct GlobalUniformData
	{
		glm::mat4 P;
		glm::mat4 V;
		glm::mat4 VP;
		glm::vec3 cameraPosition;
		float dt;
		glm::vec3 unused_;
		int nLight;
		LightData lights[128];
	} mGlobalUniformData;

	gfx::GraphicsDevice* mDevice;
	Scene* mScene;

	std::shared_ptr<gfx::Pipeline> mTrianglePipeline;
	std::shared_ptr<gfx::Pipeline> mCubemapPipeline;


	std::shared_ptr<gfx::GPUBuffer> mGlobalUniformBuffer;
	std::shared_ptr<gfx::GPUBuffer> mTransformBuffer;
	std::shared_ptr<gfx::GPUBuffer> mPerObjectDataBuffer;
	std::shared_ptr<gfx::GPUBuffer> mDrawIndirectBuffer;
	std::shared_ptr<gfx::GPUBuffer> mMaterialBuffer;

	std::shared_ptr<gfx::RenderPass> mHdrRenderPass;
	std::shared_ptr<gfx::Framebuffer> mHdrFramebuffer;
	
	gfx::Semaphore mAcquireSemaphore;
	gfx::Semaphore mReleaseSemaphore;

	gfx::Format mDepthFormat = gfx::Format::D32_SFLOAT;
	std::vector<gfx::DescriptorInfo> mDescriptorInfos;
	const int kMaxEntity = 10'000;

	std::shared_ptr<gfx::Pipeline> loadPipelines(const char* vsPath, const char* fsPath, gfx::CullMode cullMode = gfx::CullMode::Back);
	void initializeBuffers();
	void DrawCubemap(gfx::CommandList* commandList, gfx::GPUTexture* cubemap);

};
