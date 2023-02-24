#pragma once

#include "GlmIncludes.h"
#include "Graphics.h"
#include "GraphicsDevice.h"
#include "Components.h"
#include <vector>

class gfx::GpuMemoryAllocator;
class Scene;
class CascadedShadowMap;

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
	
	std::shared_ptr<CascadedShadowMap> GetShadowMap() { return mShadowMap; }

	gfx::GPUTexture* GetOutputTexture(OutputTextureType colorTextureType);

	// Bloom Setting
	void SetEnableBloom(bool state) { mEnableBloom = state; }
	void SetBlurRadius(float blurRadius) { mBloomBlurRadius = blurRadius; }
	void SetBloomThreshold(float val) { mBloomThreshold = val; }
	void SetBloomStrength(float val) { mBloomStrength = val; }
	void SetDebugCascade(bool state) { mGlobalUniformData.enableCascadeDebug = (int)state; }
	void SetEnableNormalMapping(bool state)
	{
		mGlobalUniformData.enabledNormalMapping = int(state);
	}

	virtual ~Renderer() = default;
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
		float bloomThreshold;
		int nLight;
		int enabledNormalMapping;
		int enableCascadeDebug;
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

	std::shared_ptr<gfx::Pipeline> mMeshPipeline;
	std::shared_ptr<gfx::Pipeline> mSkinnedMeshPipeline;
	std::shared_ptr<gfx::Pipeline> mCubemapPipeline;

	std::shared_ptr<gfx::GPUBuffer> mGlobalUniformBuffer;
	std::shared_ptr<gfx::GPUBuffer> mTransformBuffer;
	std::shared_ptr<gfx::GPUBuffer> mDrawIndirectBuffer;
	std::shared_ptr<gfx::GPUBuffer> mMaterialBuffer;
	std::shared_ptr<gfx::GPUBuffer> mSkinnedMatrixBuffer;

	std::shared_ptr<gfx::RenderPass> mHdrRenderPass;
	std::shared_ptr<gfx::Framebuffer> mHdrFramebuffer;
	
	gfx::Format mHDRDepthFormat = gfx::Format::D32_SFLOAT;
	gfx::Format mHDRColorFormat = gfx::Format::R16B16G16A16_SFLOAT;

	const int kMaxEntity = 10'000;
	std::shared_ptr<fx::Bloom> mBloomFX;
	std::shared_ptr<CascadedShadowMap> mShadowMap;

	std::shared_ptr<gfx::Pipeline> loadHDRPipeline(const char* vsPath, const char* fsPath, gfx::CullMode cullMode = gfx::CullMode::Back);
	void initializeBuffers();
	void DrawCubemap(gfx::CommandList* commandList, gfx::GPUTexture* cubemap);
	void DrawShadowMap(gfx::CommandList* commandList);
	void DrawBatch(gfx::CommandList* commandList, RenderBatch& batch, uint32_t lastOffset, gfx::Pipeline* pipeline);
	void DrawSkinnedMesh(gfx::CommandList* commandList, uint32_t offset);
	void DrawSkinnedShadow(gfx::CommandList* commandList);

	void CreateBatch(std::vector<DrawData>& drawDatas, std::vector<RenderBatch>& renderBatch);
	void DrawShadowBatch(gfx::CommandList* commandList, RenderBatch& renderBatch, gfx::Pipeline* pipeline, uint32_t lastOffset);

	bool mUpdateBatches = true;
	// Bloom Settings
	bool mEnableBloom = false;
	float mBloomThreshold = 1.0f;
	float mBloomBlurRadius = 10.0f;
	float mBloomStrength = 0.04f;
};
