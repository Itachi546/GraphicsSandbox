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

	uint32_t ssaoBuffer;
	uint32_t directionalShadowMap;
	float exposure;
	float globalAO;

	glm::vec3 cameraPosition;
	uint32_t nLight;

	uint32_t enableShadow;
	uint32_t enableAO;
	float bloomThreshold;
};

struct GlobalUniformData
{
	glm::mat4 P;
	glm::mat4 V;
	glm::mat4 VP;
	glm::vec3 cameraPosition;
	float dt;
};

struct Cascade {
	glm::mat4 VP;
	glm::vec4 splitDistance;
};

struct CascadeData
{
	Cascade cascades[5];
	float width;
	float height;
	int pcfSampleRadius;
	float pcfSampleRadiusMultiplier;
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
	gfx::BufferHandle mCascadeInfoBuffer;

	// @Note Temporary only used for CSM as it doesn't currently support frustum culling
	gfx::BufferHandle mIndexedIndirectCommandBuffer;

	gfx::FrameGraphBuilder mFrameGraphBuilder;
	gfx::FrameGraph mFrameGraph;
	EnvironmentData mEnvironmentData;
	bool mUseMeshShading = false;

	std::vector<RenderBatch> mDrawBatches;
	std::vector<RenderBatch> mTransparentBatches;
	std::vector<glm::vec4> projectedLightRects;

	Scene* mScene;
private:
	gfx::GraphicsDevice* mDevice;
	bool mUpdateBatches = true;
	uint32_t mSwapchainWidth;
	uint32_t mSwapchainHeight;

	std::vector<std::string> mOutputAttachments;
	uint32_t mFinalOutput;
	std::string mFinalAttachmentName;

	// Returns total number of drawElements pushed while creating batch
	uint32_t CreateBatch(std::vector<DrawData>& drawDatas, std::vector<RenderBatch>& renderBatch, uint32_t lastOffset = 0);

	gfx::RenderPassHandle mSwapchainRP;
	gfx::PipelineHandle mFullScreenPipeline;

	const uint32_t kMaxEntity = 1'000;
	const uint32_t kMaxBatches = 25;
	static const uint32_t kMaxLightBins = 16;
	const uint32_t kMaxLights = 256;

	std::array<uint32_t, kMaxLightBins> mLightLUT;

	uint32_t mBatchId = 0;
	GlobalUniformData mGlobalUniformData;
	bool mEnableDebugDraw = true;

	void InitializeBuffers();
	void AddUI();
	void UpdateLights();

};
