#pragma once

#include "GlmIncludes.h"
#include "Graphics.h"
#include "GraphicsDevice.h"
#include "Components.h"

#include <vector>

class gfx::GpuMemoryAllocator;
class Scene;

enum class OutputTextureType
{
	HDROutput,
	HDRDepth,
	HDRBrightTexture,
	BloomUpSample,
};

namespace fx {
	class Bloom;
};

class Renderer
{
public:
	Renderer();

	void Update(float dt);

	void SetScene(Scene* scene) { mScene = scene; }

	void Render(gfx::CommandList* commandList);

	void SetUpdateBatches(bool state)
	{
		mUpdateBatches = state;
	}

	gfx::GPUTexture* GetOutputTexture(OutputTextureType colorTextureType);

	// Bloom Setting
	void SetEnableBloom(bool state) { mEnableBloom = state; }
	void SetBlurRadius(float blurRadius) { mBloomBlurRadius = blurRadius; }
	void SetBloomThreshold(float val) { mBloomThreshold = val; }
	void SetBloomStrength(float val) { mBloomStrength = val; }

	void SetEnableNormalMapping(bool state)
	{
		mGlobalUniformData.enabledNormalMapping = int(state);
	}

	virtual ~Renderer() = default;
private:
	std::shared_ptr<fx::Bloom> mBloomFX;

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
		float bloomThreshold;
		int nLight;
		int enabledNormalMapping;
		float unused_;
		LightData lights[128];
	} mGlobalUniformData;

	struct RenderBatch
	{
		std::vector<gfx::DrawIndirectCommand> drawCommands;
		std::vector<glm::mat4> transforms;
		std::vector<MaterialComponent> materials;
		std::array<gfx::GPUTexture, 64> textures;
		uint32_t textureCount = 0;
		gfx::BufferView vertexBuffer;
		gfx::BufferView indexBuffer;
	};
	std::vector<RenderBatch> mRenderBatches;

	gfx::GraphicsDevice* mDevice;
	Scene* mScene;

	std::shared_ptr<gfx::Pipeline> mTrianglePipeline;
	std::shared_ptr<gfx::Pipeline> mCubemapPipeline;

	std::shared_ptr<gfx::GPUBuffer> mGlobalUniformBuffer;
	std::shared_ptr<gfx::GPUBuffer> mTransformBuffer;
	std::shared_ptr<gfx::GPUBuffer> mDrawIndirectBuffer;
	std::shared_ptr<gfx::GPUBuffer> mMaterialBuffer;

	std::shared_ptr<gfx::RenderPass> mHdrRenderPass;
	std::shared_ptr<gfx::Framebuffer> mHdrFramebuffer;
	
	gfx::Format mHDRDepthFormat = gfx::Format::D32_SFLOAT;
	gfx::Format mHDRColorFormat = gfx::Format::R16B16G16A16_SFLOAT;

	// fx Variables
	const int kMaxEntity = 10'000;

	std::shared_ptr<gfx::Pipeline> loadHDRPipeline(const char* vsPath, const char* fsPath, gfx::CullMode cullMode = gfx::CullMode::Back);
	void initializeBuffers();
	void DrawCubemap(gfx::CommandList* commandList, gfx::GPUTexture* cubemap);
	void DrawBatch(gfx::CommandList* commandList, RenderBatch& batch, uint32_t lastOffset, gfx::GpuMemoryAllocator* allocator);
	void CreateBatch();

	bool mUpdateBatches = true;
	// Bloom Settings
	bool mEnableBloom = false;
	float mBloomThreshold = 1.0f;
	float mBloomBlurRadius = 10.0f;
	float mBloomStrength = 0.04f;
};
