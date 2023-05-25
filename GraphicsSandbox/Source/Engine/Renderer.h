#pragma once

#include "GlmIncludes.h"
#include "Graphics.h"
#include "GraphicsDevice.h"
#include "Components.h"
#include "Resource.h"
#include "FrameGraph.h"

#include <vector>
#include <array>


class Scene;
class CascadedShadowMap;

struct RenderBatch
{
	std::vector<gfx::DrawIndirectCommand> drawCommands;
	std::vector<glm::mat4> transforms;
	std::vector<MaterialComponent> materials;
	uint32_t textureCount = 0;
	gfx::BufferView vertexBuffer;
	gfx::BufferView indexBuffer;
};

struct LightData
{
	glm::vec3 position;
	float radius;
	glm::vec3 color;
	float lightType;
};

struct EnvironmentData {
	uint32_t nLight;
	uint32_t irradianceMap;
	uint32_t prefilterEnvMap;
	uint32_t brdfLUT;

	uint32_t positionBuffer;
	uint32_t normalBuffer;
	uint32_t pbrBuffer;
	uint32_t colorBuffer;

	glm::vec3 cameraPosition;
	float exposure;
	float globalAO;
};

struct GlobalUniformData
{
	glm::mat4 P;
	glm::mat4 V;
	glm::mat4 VP;
	glm::vec3 cameraPosition;
	float dt;
};

class Renderer
{
public:
	Renderer(uint32_t width, uint32_t height);

	void Update(float dt);

	void SetScene(Scene* scene);

	void Render(gfx::CommandList* commandList);

	//gfx::TextureHandle GetOutputTexture(OutputTextureType colorTextureType);
	void onResize(uint32_t width, uint32_t height);

	void Shutdown();

	void CreateBatch(std::vector<DrawData>& drawDatas, std::vector<RenderBatch>& renderBatch);

	virtual ~Renderer() = default;

	gfx::BufferHandle mLightBuffer;
	gfx::BufferHandle mGlobalUniformBuffer;
	gfx::BufferHandle mTransformBuffer;
	gfx::BufferHandle mDrawIndirectBuffer;
	gfx::BufferHandle mMaterialBuffer;
	gfx::BufferHandle mSkinnedMatrixBuffer;
	gfx::FrameGraphBuilder mFrameGraphBuilder;
	gfx::FrameGraph mFrameGraph;
	EnvironmentData mEnvironmentData;
private:

	gfx::GraphicsDevice* mDevice;
	Scene* mScene;

	std::vector<std::string> mOutputAttachments;
	uint32_t mFinalOutput;

	gfx::PipelineHandle mCubemapPipeline;
	gfx::RenderPassHandle mSwapchainRP;
	gfx::PipelineHandle mFullScreenPipeline;

	const int kMaxEntity = 10'000;
	GlobalUniformData mGlobalUniformData;

	void InitializeBuffers();
	void AddUI();
	void DrawCubemap(gfx::CommandList* commandList, gfx::TextureHandle cubemap);

	/*
	gfx::PipelineHandle loadHDRPipeline(const char* vsPath, const char* fsPath, gfx::CullMode cullMode = gfx::CullMode::Back);

	void DrawShadowMap(gfx::CommandList* commandList);
	void DrawBatch(gfx::CommandList* commandList, RenderBatch& batch, uint32_t lastOffset, gfx::PipelineHandle pipeline, bool shadowPass = false);
	void DrawSkinnedMesh(gfx::CommandList* commandList, uint32_t offset, gfx::PipelineHandle pipeline, bool shadowPass = false);
	uint32_t addUnique(std::array<gfx::TextureHandle, 64>& textures, uint32_t& lastIndex, gfx::TextureHandle texture);
	*/
};
