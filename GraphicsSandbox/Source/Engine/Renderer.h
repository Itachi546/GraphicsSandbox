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
	gfx::BufferView vertexBuffer;
	gfx::BufferView indexBuffer;
	gfx::BufferHandle meshletVertexBuffer;
	gfx::BufferHandle meshletTriangleBuffer;
	gfx::BufferHandle meshletBuffer;
	uint32_t meshletCount;

	// Offset in number terms of number of element before this
	// Used to offset in transform/material buffer and is assumed to have one to one mapping between mesh and material
	uint32_t offset;
	// Count of number of transform/material/drawCommands in the batch
	uint32_t count;

	// Id in increasing order and may be unique every frame
	uint32_t id;
};

struct LightData
{
	glm::vec3 position;
	float radius;
	glm::vec3 color;
	float lightType;
};

struct EnvironmentData {
	uint32_t irradianceMap;
	uint32_t prefilterEnvMap;
	uint32_t brdfLUT;

	uint32_t positionBuffer;
	uint32_t normalBuffer;
	uint32_t pbrBuffer;
	uint32_t colorBuffer;
	uint32_t emissiveBuffer;

	glm::vec3 cameraPosition;
	float exposure;
	float globalAO;
	uint32_t nLight;

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

	virtual ~Renderer() = default;

	gfx::BufferHandle mLightBuffer;
	gfx::BufferHandle mGlobalUniformBuffer;
	gfx::BufferHandle mTransformBuffer;
	gfx::BufferHandle mDrawIndirectBuffer;
	gfx::BufferHandle mMaterialBuffer;
	gfx::BufferHandle mSkinnedMatrixBuffer;
	gfx::BufferHandle mMeshDrawDataBuffer;
	gfx::BufferHandle mDrawCommandCountBuffer;

	gfx::FrameGraphBuilder mFrameGraphBuilder;
	gfx::FrameGraph mFrameGraph;
	EnvironmentData mEnvironmentData;
	bool mUseMeshShading = false;

	std::vector<RenderBatch> mDrawBatches;
	std::vector<RenderBatch> mTransparentBatches;

private:

	gfx::GraphicsDevice* mDevice;
	Scene* mScene;
	bool mUpdateBatches = true;


	std::vector<std::string> mOutputAttachments;
	uint32_t mFinalOutput;

	// Returns total number of drawElements pushed while creating batch
	uint32_t CreateBatch(std::vector<DrawData>& drawDatas, std::vector<RenderBatch>& renderBatch, uint32_t lastOffset = 0);

	gfx::PipelineHandle mCubemapPipeline;
	gfx::RenderPassHandle mSwapchainRP;
	gfx::PipelineHandle mFullScreenPipeline;

	const int kMaxEntity = 1'000;
	const int kMaxBatches = 25;
	uint32_t mBatchId = 0;
	GlobalUniformData mGlobalUniformData;
	bool mEnableDebugDraw = false;

	void InitializeBuffers();
	void AddUI();
	void DrawCubemap(gfx::CommandList* commandList, gfx::TextureHandle cubemap);
	void UpdateLights();

	/*
	gfx::PipelineHandle loadHDRPipeline(const char* vsPath, const char* fsPath, gfx::CullMode cullMode = gfx::CullMode::Back);

	void DrawShadowMap(gfx::CommandList* commandList);
	void DrawBatch(gfx::CommandList* commandList, RenderBatch& batch, uint32_t lastOffset, gfx::PipelineHandle pipeline, bool shadowPass = false);
	void DrawSkinnedMesh(gfx::CommandList* commandList, uint32_t offset, gfx::PipelineHandle pipeline, bool shadowPass = false);
	uint32_t addUnique(std::array<gfx::TextureHandle, 64>& textures, uint32_t& lastIndex, gfx::TextureHandle texture);
	*/
};
