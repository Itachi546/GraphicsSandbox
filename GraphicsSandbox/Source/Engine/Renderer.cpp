#include "Renderer.h"
#include "Utils.h"
#include "Scene.h"
#include "Input.h"
#include "Logger.h"
#include "Profiler.h"
#include "StringConstants.h"
#include "DebugDraw.h"
#include "TransformComponent.h"
#include "TextureCache.h"
#include "GUI/ImGuiService.h"
#include "MeshData.h"

#include "Pass/Pass.h"

#include <vector>
#include <algorithm>

struct SortedLight {
	uint32_t id;
	float projectedZ;
	float projectedZMin;
	float projectedZMax;
};


Renderer::Renderer(uint32_t width, uint32_t height) : mDevice(gfx::GetDevice()), mSwapchainWidth(width), mSwapchainHeight(height)
{
	// Create SwapchainPipeline
	uint32_t vertexLen = 0, fragmentLen = 0;

	ShaderPathInfo* shaderPathInfo = ShaderPath::get("fullscreen_pass");
	char* vertexCode = Utils::ReadFile(shaderPathInfo->shaders[0], &vertexLen);
	char* fragmentCode = Utils::ReadFile(shaderPathInfo->shaders[1], &fragmentLen);
	assert(vertexCode != nullptr);
	assert(fragmentCode != nullptr);

	gfx::PipelineDesc pipelineDesc = {};
	std::vector<gfx::ShaderDescription> shaders(2);
	shaders[0] = { vertexCode, vertexLen };
	shaders[1] = { fragmentCode, fragmentLen };
	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders.data();

	pipelineDesc.renderPass = mSwapchainRP;
	pipelineDesc.rasterizationState.enableDepthTest = false;
	pipelineDesc.rasterizationState.enableDepthWrite = false;
	pipelineDesc.rasterizationState.cullMode = gfx::CullMode::None;
	gfx::BlendState blendState = {};
	pipelineDesc.blendStates = &blendState;
	pipelineDesc.blendStateCount = 1;

	mFullScreenPipeline = mDevice->CreateGraphicsPipeline(&pipelineDesc);
	delete[] vertexCode;
	delete[] fragmentCode;

	InitializeBuffers();

	mFrameGraphBuilder.Init(mDevice);
	mFrameGraph.Init(&mFrameGraphBuilder);
	mFrameGraph.Parse("GraphicsSandbox/Shaders/graph.json");
	mFrameGraph.Compile();

	mFrameGraphBuilder.GetAllTextureResourceName(mOutputAttachments);

	// Remove layered texture
	mOutputAttachments.erase(std::remove_if(mOutputAttachments.begin(), mOutputAttachments.end(), [&](const std::string& attachment) {
		return mFrameGraphBuilder.AccessResource(attachment)->info.texture.layerCount > 1;
		}), mOutputAttachments.end());

	auto RegisterPass = [&](const std::string name, gfx::FrameGraphPass* pass) {
		gfx::FrameGraphNode* node = mFrameGraphBuilder.AccessNode(name);
		if (node == nullptr) return;
		mFrameGraph.RegisterRenderer(name, pass);
		pass->Initialize(node->renderPass);
	};

	DebugDraw::Initialize(mSwapchainRP);
	DebugDraw::SetEnable(mEnableDebugDraw);

	RegisterPass("depth_pre_pass", new gfx::DepthPrePass(this));
	RegisterPass("gbuffer_pass", new gfx::GBufferPass(this));
	RegisterPass("lighting_pass", new gfx::LightingPass(this));
	RegisterPass("transparent_pass", new gfx::TransparentPass(this));
	RegisterPass("fxaa_pass", new gfx::FXAAPass(this, width, height));
	RegisterPass("drawcull_pass", new gfx::DrawCullPass(this));
	RegisterPass("ssao_pass", new gfx::SSAO(this));

	// @NOTE For ComputePass with ImageStorage access layout we have to manually transition the image layout
	gfx::BlurPass* blurPass = new gfx::BlurPass(this, 960, 540);
	blurPass->SetInputTexture(mFrameGraphBuilder.AccessResource("ssao")->info.texture.texture);
	blurPass->SetOutputTexture(mFrameGraphBuilder.AccessResource("ssao_blur")->info.texture.texture);
	RegisterPass("ssao_blur_pass", blurPass);

	gfx::CascadedShadowPass* csmShadowPass = new gfx::CascadedShadowPass(this);
	RegisterPass("cascaded_shadow_pass", csmShadowPass);
	gfx::FrameGraphResourceInfo& csmInfo =  mFrameGraphBuilder.AccessResource("csm_depth")->info;
	csmShadowPass->SetShadowDims(csmInfo.texture.width, csmInfo.texture.height);

	auto found = std::find(mOutputAttachments.begin(), mOutputAttachments.end(), "lighting");
	if (found != mOutputAttachments.end())
		mFinalOutput = (uint32_t)std::distance(mOutputAttachments.begin(), found);
	else
		mFinalOutput = 0;
}


void Renderer::SetScene(Scene* scene)
{
	mScene = scene;
	// Update Environment data
	mEnvironmentData.exposure = 1.0f;
	auto& env = mScene->GetEnvironmentMap();
	mEnvironmentData.brdfLUT = env->GetBRDFLUT().handle;
	mEnvironmentData.irradianceMap = env->GetIrradianceMap().handle;
	mEnvironmentData.prefilterEnvMap = env->GetPrefilterMap().handle;
	mEnvironmentData.pbrBuffer = mFrameGraphBuilder.AccessResource("gbuffer_metallic_roughness_occlusion")->info.texture.texture.handle;
	mEnvironmentData.colorBuffer = mFrameGraphBuilder.AccessResource("gbuffer_colour")->info.texture.texture.handle;
	mEnvironmentData.normalBuffer = mFrameGraphBuilder.AccessResource("gbuffer_normals")->info.texture.texture.handle;
	mEnvironmentData.positionBuffer = mFrameGraphBuilder.AccessResource("gbuffer_position")->info.texture.texture.handle;
	mEnvironmentData.emissiveBuffer = mFrameGraphBuilder.AccessResource("gbuffer_emissive")->info.texture.texture.handle;
	mEnvironmentData.directionalShadowMap = mFrameGraphBuilder.AccessResource("csm_depth")->info.texture.texture.handle;
	mEnvironmentData.ssaoBuffer = mFrameGraphBuilder.AccessResource("ssao_blur")->info.texture.texture.handle;
	mEnvironmentData.globalAO = 0.2f;
	mEnvironmentData.enableShadow = 1;
	mEnvironmentData.enableAO = 1;
}

// TODO: temp width and height variable
void Renderer::Update(float dt)
{
	mUpdateBatches = true;

	// Update Global Uniform Data
	auto compMgr = mScene->GetComponentManager();
	Camera* camera = mScene->GetCamera();
	glm::mat4 P = camera->GetProjectionMatrix();
	mGlobalUniformData.P = P;
	glm::mat4 V = camera->GetViewMatrix();
	mGlobalUniformData.V = V;
	mGlobalUniformData.VP = P * V;
	mGlobalUniformData.cameraPosition = camera->GetPosition();
	mGlobalUniformData.dt += dt;
	mDevice->CopyToBuffer(mGlobalUniformBuffer, &mGlobalUniformData, 0, sizeof(GlobalUniformData));

	UpdateLights();

	// Update Environment data
	mEnvironmentData.cameraPosition = mScene->GetCamera()->GetPosition();

	// Update Batch if needed
	if (mUpdateBatches) {
		mDrawBatches.clear();
		mTransparentBatches.clear();

		std::vector<DrawData> opaque;
		std::vector<DrawData> transparent;
		mScene->GenerateDrawData(opaque, transparent);

		mBatchId = 0;
		uint32_t lastOffset = 0;
		lastOffset = CreateBatch(opaque, mDrawBatches, lastOffset);
		lastOffset = CreateBatch(transparent, mTransparentBatches, lastOffset);
		mUpdateBatches = false;
	}
}


void Renderer::Render(gfx::CommandList* commandList)
{
	// New GUI Frame
	ui::ImGuiService::GetInstance()->NewFrame();
	ImGui::Begin("Renderer");

	AddUI();
	mScene->AddUI();

	for (uint32_t i = 0; i < mFrameGraph.nodeHandles.size(); ++i)
	{
		gfx::FrameGraphNode* node = mFrameGraphBuilder.AccessNode(mFrameGraph.nodeHandles[i]);
		if (!node->enabled) continue;

		// Begin GPU Timer
		RangeId nodeProfilerId = Profiler::StartRangeGPU(commandList, node->name.c_str());
		mDevice->BeginDebugLabel(commandList, node->name.c_str());

		std::vector<gfx::ResourceBarrierInfo> colorAttachmentBarrier;
		std::vector<gfx::ResourceBarrierInfo> depthAttachmentBarrier;

		// Generate the Image barrier from input and output
		for (uint32_t i = 0; i < node->inputs.size(); ++i)
		{
			gfx::FrameGraphResource* input = mFrameGraphBuilder.AccessResource(node->inputs[i]);
			gfx::FrameGraphResourceInfo resourceInfo = input->info;
			if (input->type == gfx::FrameGraphResourceType::Texture)
			{
				if (resourceInfo.texture.imageAspect == gfx::ImageAspect::Depth)
				{
					gfx::ResourceBarrierInfo barrierInfo = gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None,
						gfx::AccessFlag::DepthStencilWrite,
						gfx::ImageLayout::DepthAttachmentOptimal,
						resourceInfo.texture.texture);
					depthAttachmentBarrier.push_back(barrierInfo);
				}
				else {
					gfx::ResourceBarrierInfo barrierInfo = gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::ColorAttachmentWrite,
						gfx::AccessFlag::ShaderRead,
						gfx::ImageLayout::ShaderReadOptimal,
						resourceInfo.texture.texture );
					colorAttachmentBarrier.push_back(barrierInfo);
				}
			}
			else if (input->type == gfx::FrameGraphResourceType::Attachment) {
				if (resourceInfo.texture.imageAspect == gfx::ImageAspect::Depth)
				{
					gfx::ResourceBarrierInfo barrierInfo = gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None,
						gfx::AccessFlag::DepthStencilRead,
						gfx::ImageLayout::DepthAttachmentOptimal,
						resourceInfo.texture.texture);
					depthAttachmentBarrier.push_back(barrierInfo);
				}
			}
		}

		// Pipeline barrier for color attachments
		// Color inputs are always an output of ColorAttachment and used in Fragment shader
		if (colorAttachmentBarrier.size() > 0)
		{
			gfx::PipelineBarrierInfo pipelineBarrier = { colorAttachmentBarrier.data(), (uint32_t)colorAttachmentBarrier.size(), gfx::PipelineStage::ColorAttachmentOutput, gfx::PipelineStage::FragmentShader };
			mDevice->PipelineBarrier(commandList, &pipelineBarrier);
			colorAttachmentBarrier.clear();
		}

		// Pipeline barrier for depth attachments
		// Depth attachment are output of the depth testing and further used as depth testing
		if (depthAttachmentBarrier.size() > 0)
		{
			gfx::PipelineBarrierInfo pipelineBarrier = { depthAttachmentBarrier.data(), (uint32_t)depthAttachmentBarrier.size(), gfx::PipelineStage::LateFragmentTest, gfx::PipelineStage::EarlyFramentTest };
			mDevice->PipelineBarrier(commandList, &pipelineBarrier);
			depthAttachmentBarrier.clear();
		}

		for (uint32_t i = 0; i < node->outputs.size(); ++i)
		{
			gfx::FrameGraphResource* output = mFrameGraphBuilder.AccessResource(node->outputs[i]);
			gfx::FrameGraphResourceInfo resourceInfo = output->info;

			if (output->type == gfx::FrameGraphResourceType::Attachment)
			{
				if (resourceInfo.texture.imageAspect == gfx::ImageAspect::Depth)
				{
					gfx::ResourceBarrierInfo barrierInfo = gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None,
						gfx::AccessFlag::DepthStencilWrite,
						gfx::ImageLayout::DepthAttachmentOptimal,
						resourceInfo.texture.texture);
					depthAttachmentBarrier.push_back(barrierInfo);
				}
				else {
					gfx::ResourceBarrierInfo barrierInfo = gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None,
						gfx::AccessFlag::ColorAttachmentWrite,
						gfx::ImageLayout::ColorAttachmentOptimal,
						resourceInfo.texture.texture);
					colorAttachmentBarrier.push_back(barrierInfo);
				}
			}
		}

		// Pipeline barrier for color attachments
		if (colorAttachmentBarrier.size() > 0)
		{
			gfx::PipelineBarrierInfo pipelineBarrier = { colorAttachmentBarrier.data(), (uint32_t)colorAttachmentBarrier.size(), gfx::PipelineStage::ColorAttachmentOutput, gfx::PipelineStage::ColorAttachmentOutput };
			mDevice->PipelineBarrier(commandList, &pipelineBarrier);
		}

		// Pipeline barrier for depth attachments
		if (depthAttachmentBarrier.size() > 0)
		{
			gfx::PipelineBarrierInfo pipelineBarrier = { depthAttachmentBarrier.data(), (uint32_t)depthAttachmentBarrier.size(), gfx::PipelineStage::LateFragmentTest, gfx::PipelineStage::EarlyFramentTest };
			mDevice->PipelineBarrier(commandList, &pipelineBarrier);
		}

		node->renderer->PreRender(commandList);
		if (node->compute) {
			node->renderer->Render(commandList, mScene);
		}
		else {
			mDevice->BeginRenderPass(commandList, node->renderPass, node->framebuffer);
			node->renderer->Render(commandList, mScene);

			if (node->name == "transparent_pass") {
				DebugDraw::Draw(commandList, mGlobalUniformData.VP, mGlobalUniformData.cameraPosition);
			}

			mDevice->EndRenderPass(commandList);
		}
		// End GPU Timer
		Profiler::EndRangeGPU(commandList, nodeProfilerId);
		// Draw UI
		node->renderer->AddUI();
		mDevice->EndDebugLabel(commandList);
	}
	ImGui::End();

	RangeId start = Profiler::StartRangeGPU(commandList, "fullscreen_pass");
	mDevice->BeginDebugLabel(commandList, "fullscreen_pass");

	gfx::TextureHandle outputTexture = mFrameGraphBuilder.
		AccessResource(mOutputAttachments[mFinalOutput])->info.
		texture.texture;

	gfx::ResourceBarrierInfo imageBarrier = gfx::ResourceBarrierInfo::CreateImageBarrier(
		gfx::AccessFlag::None,
		gfx::AccessFlag::ShaderRead,
		gfx::ImageLayout::ShaderReadOptimal,
		outputTexture
	);

	gfx::PipelineBarrierInfo pipelineBarrier = {
		&imageBarrier,
		1,
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::FragmentShader
	};

	mDevice->PipelineBarrier(commandList, &pipelineBarrier);

	mDevice->BeginRenderPass(commandList, mSwapchainRP, gfx::INVALID_FRAMEBUFFER);
	gfx::DescriptorInfo descriptorInfo = { &outputTexture, 0, 0, gfx::DescriptorType::Image };

	uint32_t depth = 4;
	if (mFinalAttachmentName == "ssao" || mFinalAttachmentName == "depth" || mFinalAttachmentName == "ssao_blur")
		depth = 1;
	mDevice->PushConstants(commandList, mFullScreenPipeline, gfx::ShaderStage::Fragment, &depth, sizeof(uint32_t));

	mDevice->UpdateDescriptor(mFullScreenPipeline, &descriptorInfo, 1);
	mDevice->BindPipeline(commandList, mFullScreenPipeline);
	mDevice->Draw(commandList, 6, 0, 1);
	Profiler::EndRangeGPU(commandList, start);

	// Draw GUI
	start = Profiler::StartRangeGPU(commandList, "imgui");
	ui::ImGuiService::GetInstance()->Render(commandList);

	mDevice->EndRenderPass(commandList);
	Profiler::EndRangeGPU(commandList, start);
	mDevice->EndDebugLabel(commandList);
}

/*
void Renderer::DrawShadowMap(gfx::CommandList* commandList)
{
	// BeginShadow Pass
	auto rangeId = Profiler::StartRangeGPU(commandList, "Shadow Pass");
	mShadowMap->BeginRender(commandList);

	uint32_t lastOffset = 0;
	auto pipeline = mShadowMap->mPipeline;
	for (auto& batch : mRenderBatches)
	{
		if (batch.drawCommands.size() > 0)
		{
			DrawBatch(commandList, batch, lastOffset, pipeline, true);
			lastOffset += (uint32_t)batch.drawCommands.size();
		}
	}

	DrawSkinnedMesh(commandList, 0, mShadowMap->mSkinnedPipeline, true);

	mShadowMap->EndRender(commandList);
	Profiler::EndRangeGPU(commandList, rangeId);
}
*/

/*
void Renderer::Render(gfx::CommandList* commandList)
{

	{
		// Draw Shadow Map
		DrawShadowMap(commandList);

		gfx::ImageBarrierInfo shadowBarrier = {
				gfx::ImageBarrierInfo{gfx::AccessFlag::DepthStencilWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, mDevice->GetFramebufferAttachment(mShadowMap->mFramebuffer, 0)}
		};
		gfx::PipelineBarrierInfo barrier = { &shadowBarrier, 1, gfx::PipelineStage::LateFramentTest, gfx::PipelineStage::FragmentShader};
		mDevice->PipelineBarrier(commandList, &barrier);
	}
	*/
	// Begin HDR RenderPass
	/*
	{
		gfx::ImageBarrierInfo barriers[] = {
			gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ColorAttachmentWrite, gfx::ImageLayout::ColorAttachmentOptimal, mDevice->GetFramebufferAttachment(mHdrFramebuffer, 0)},
			gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ColorAttachmentWrite, gfx::ImageLayout::ColorAttachmentOptimal, mDevice->GetFramebufferAttachment(mHdrFramebuffer, 1)},
			gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::DepthStencilWrite, gfx::ImageLayout::DepthAttachmentOptimal, mDevice->GetFramebufferAttachment(mHdrFramebuffer, 2)},
		};

		gfx::PipelineBarrierInfo colorAttachmentBarrier = { barriers, 2, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::ColorAttachmentOutput };
		gfx::PipelineBarrierInfo depthAttachmentBarrier = { &barriers[2], 1, gfx::PipelineStage::BottomOfPipe, gfx::PipelineStage::EarlyFramentTest };
		mDevice->PipelineBarrier(commandList, &colorAttachmentBarrier);
		mDevice->PipelineBarrier(commandList, &depthAttachmentBarrier);

		auto hdrPass = Profiler::StartRangeGPU(commandList, "HDR Pass");
		mDevice->BeginRenderPass(commandList, mHdrRenderPass, mHdrFramebuffer);
		*/
		/*
		* TODO: Currently PerObjectData is extracted from the
		* DrawData by iterating through it. In future maybe
		* we can do culling in the compute shader and set
		* PerObjectBuffer from compute shader.
		*/
	//mDevice->BeginDebugMarker(commandList, "Draw Objects", 1.0f, 1.0f, 1.0f, 1.0f);

		// offset is used to keep track of the buffer offset of transform data
		// so as we don't have to use absolute index but relative
		// We have all transform and material data in same buffer loaded at one
		// and the buffer is binded relatively according to the batch
	/*
		uint32_t offset = 0;
		for (auto& batch : mRenderBatches)
		{
			DrawBatch(commandList, batch, offset, mMeshPipeline);
			// NOTE: for now the size of DrawCommand, Transform and Material is same
			// We need to create offset within the batch later if we decide to share
			// materials
			offset += (uint32_t)batch.drawCommands.size();
		}

		// Draw Skinned Mesh
		DrawSkinnedMesh(commandList, offset, mSkinnedMeshPipeline);

		mDevice->EndDebugMarker(commandList);

		// Draw Cubemap
		auto& envMap = mScene->GetEnvironmentMap();
		gfx::TextureHandle cubemap = envMap->GetCubemap();

		mDevice->BeginDebugMarker(commandList, "Draw Cubemap", 1.0f, 1.0f, 1.0f, 1.0f);
		DrawCubemap(commandList, envMap->GetCubemap());
		mDevice->EndDebugMarker(commandList);
		mDevice->EndRenderPass(commandList);
		Profiler::EndRangeGPU(commandList, hdrPass);
	}

	if (mEnableBloom)
	{
		auto bloomPass = Profiler::StartRangeGPU(commandList, "Bloom Pass");
		// Bloom Pass
		mBloomFX->Generate(commandList, mDevice->GetFramebufferAttachment(mHdrFramebuffer, 1), mBloomBlurRadius);
		mBloomFX->Composite(commandList, mDevice->GetFramebufferAttachment(mHdrFramebuffer, 0), mBloomStrength);
		Profiler::EndRangeGPU(commandList, bloomPass);
	}

}
/*
gfx::TextureHandle Renderer::GetOutputTexture(OutputTextureType colorTextureType)
{
	switch (colorTextureType)
	{
	case OutputTextureType::HDROutput:
		return mDevice->GetFramebufferAttachment(mHdrFramebuffer, 0);
	case OutputTextureType::HDRBrightTexture:
		return mDevice->GetFramebufferAttachment(mHdrFramebuffer, 1);
	case OutputTextureType::HDRDepth:
		return mDevice->GetFramebufferAttachment(mHdrFramebuffer, 2);
	default:
		assert(!"Undefined output texture");
		return gfx::INVALID_TEXTURE;
	}
}
*/
/*
gfx::PipelineHandle Renderer::loadHDRPipeline(const char* vsPath, const char* fsPath, gfx::CullMode cullMode)
{
	// Create PBR Pipeline
	uint32_t vertexLen = 0, fragmentLen = 0;
	char* vertexCode = Utils::ReadFile(vsPath, &vertexLen);
	char* fragmentCode = Utils::ReadFile(fsPath, &fragmentLen);
	assert(vertexCode != nullptr);
	assert(fragmentCode != nullptr);

	gfx::PipelineDesc pipelineDesc = {};
	std::vector<gfx::ShaderDescription> shaders(2);
	shaders[0] = { vertexCode, vertexLen };
	shaders[1] = { fragmentCode, fragmentLen };
	pipelineDesc.shaderCount = 2;
	pipelineDesc.shaderDesc = shaders.data();

	pipelineDesc.renderPass = mHdrRenderPass;
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.rasterizationState.cullMode = cullMode;

	gfx::BlendState blendState[2] = {};
	pipelineDesc.blendState = blendState;
	pipelineDesc.blendStateCount = static_cast<uint32_t>(std::size(blendState));

	gfx::PipelineHandle handle = mDevice->CreateGraphicsPipeline(&pipelineDesc);
	delete[] vertexCode;
	delete[] fragmentCode;
	return handle;
}
*/
void Renderer::InitializeBuffers()
{
	// Global Uniform Buffer
	gfx::GPUBufferDesc uniformBufferDesc = {};
	uniformBufferDesc.bindFlag = gfx::BindFlag::ConstantBuffer;
	uniformBufferDesc.usage = gfx::Usage::Upload;
	uniformBufferDesc.size = sizeof(GlobalUniformData);
	mGlobalUniformBuffer = mDevice->CreateBuffer(&uniformBufferDesc);

	// Skinned Matrix Buffer;
	uniformBufferDesc.size = sizeof(glm::mat4) * MAX_BONE_COUNT * 10;
	mSkinnedMatrixBuffer = mDevice->CreateBuffer(&uniformBufferDesc);

	// Per Object Data Buffer
	gfx::GPUBufferDesc bufferDesc = {};
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = kMaxEntity * sizeof(glm::mat4);
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	mTransformBuffer = mDevice->CreateBuffer(&bufferDesc);

	// Material Component Buffer
	bufferDesc.size = kMaxEntity * sizeof(MaterialComponent);
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	mMaterialBuffer = mDevice->CreateBuffer(&bufferDesc);

	// Light Data Buffer
	bufferDesc.size = sizeof(LightData) * kMaxLights;
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	mLightBuffer = mDevice->CreateBuffer(&bufferDesc);

	// MeshDraw Data Buffer
	bufferDesc.size = kMaxEntity * sizeof(MeshDrawData);
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	mMeshDrawDataBuffer = mDevice->CreateBuffer(&bufferDesc);

	// Draw Indirect Buffer
	bufferDesc.usage = gfx::Usage::Default;
	bufferDesc.size = kMaxEntity * sizeof(gfx::MeshDrawIndirectCommand);
	bufferDesc.bindFlag = gfx::BindFlag::IndirectBuffer | gfx::BindFlag::ShaderResource;
	mDrawIndirectBuffer = mDevice->CreateBuffer(&bufferDesc);

	bufferDesc.size = kMaxEntity * sizeof(gfx::DrawIndirectCommand);
	mIndexedIndirectCommandBuffer = mDevice->CreateBuffer(&bufferDesc);

	// DrawCommand Count Buffer
	bufferDesc.size = sizeof(uint32_t) * kMaxBatches;
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource | gfx::BindFlag::IndirectBuffer;
	mDrawCommandCountBuffer = mDevice->CreateBuffer(&bufferDesc);

	// CascadeInfo bufferxs
	bufferDesc.bindFlag = gfx::BindFlag::ConstantBuffer;
	bufferDesc.size = sizeof(CascadeData);
	bufferDesc.usage = gfx::Usage::Upload;
	mCascadeInfoBuffer = mDevice->CreateBuffer(&bufferDesc);
}

void Renderer::AddUI()
{
	ImGui::Separator();
	if (ImGui::Checkbox("DebugDraw", &mEnableDebugDraw))
		DebugDraw::SetEnable(mEnableDebugDraw);
	if (mDevice->SupportMeshShading())
		ImGui::Checkbox("Mesh Shading", &mUseMeshShading);
	ImGui::SliderFloat("globalAOMultiplier", &mEnvironmentData.globalAO, 0.0f, 1.0f);
	ImGui::SliderFloat("exposure", &mEnvironmentData.exposure, 0.0f, 4.0f);

	static bool enableShadow = mEnvironmentData.enableShadow > 0 ? true : false;
	if (ImGui::Checkbox("Shadow", &enableShadow)) {
		mEnvironmentData.enableShadow = uint32_t(enableShadow);
		mFrameGraphBuilder.AccessNode("cascaded_shadow_pass")->enabled = enableShadow;
	}

	static bool enableAO = mEnvironmentData.enableAO > 0 ? true : false;
	if (ImGui::Checkbox("AO", &enableAO)) {
		mEnvironmentData.enableAO = uint32_t(enableAO);
	}

	ImGui::Text("Visible Light: %d", (uint32_t)projectedLightRects.size());

	if (ImGui::BeginCombo("Final Output", mOutputAttachments[mFinalOutput].c_str()))
	{
		for (uint32_t i = 0; i < mOutputAttachments.size(); ++i)
		{
			const bool isSelected = (i == mFinalOutput);

			if (ImGui::Selectable(mOutputAttachments[i].c_str(), isSelected)) {
				mFinalOutput = i;
				mFinalAttachmentName = mOutputAttachments[i];
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	/*
    const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetWindowSize(io.DisplaySize);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
	ImGui::PushStyleColor(ImGuiCol_Border, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

	ImGui::Begin("LightRect");
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);

	
	for (auto& rect : projectedLightRects) {
		drawList->AddRectFilled(ImVec2{ rect.x, rect.y }, ImVec2{ rect.z, rect.w }, ImColor(1.0f, 0.0f, 0.0f, 0.1f));
	};
	*/

	/*
	const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetWindowSize(io.DisplaySize);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
	ImGui::PushStyleColor(ImGuiCol_Border, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

	
	ImGui::Begin("LightRect");
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(2);

	for (auto& rect : projectedLightRects) {
		drawList->AddRectFilled(ImVec2{ rect.x, rect.y }, ImVec2{ rect.z, rect.w }, ImColor(1.0f, 0.0f, 0.0f, 0.1f));
	};
	*/
	/*
	gfx::FrameGraphResource* csmDepth = mFrameGraphBuilder.AccessResource("csm_depth");
	if (csmDepth) {
		ui::ImGuiService::GetInstance()->AddImage(csmDepth->info.texture.texture, ImVec2(256.0f, 256.0f));
	}*/
}

void Renderer::DrawCubemap(gfx::CommandList* commandList, gfx::TextureHandle cubemap)
{
	MeshRenderer* meshRenderer = mScene->mComponentManager->GetComponent<MeshRenderer>(mScene->mCube);

	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[3] = {};
	descriptorInfos[0].buffer = mGlobalUniformBuffer;
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].size = sizeof(GlobalUniformData);
	descriptorInfos[0].type = gfx::DescriptorType::UniformBuffer;

	auto& vb = meshRenderer->vertexBuffer;
	auto& ib = meshRenderer->indexBuffer;

	descriptorInfos[1].buffer = vb.buffer;
	descriptorInfos[1].offset = vb.byteOffset;
	descriptorInfos[1].size = vb.byteLength;
	descriptorInfos[1].type = gfx::DescriptorType::StorageBuffer;

	descriptorInfos[2].texture = &cubemap;
	descriptorInfos[2].offset = 0;
	descriptorInfos[2].type = gfx::DescriptorType::Image;

	mDevice->UpdateDescriptor(mCubemapPipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(commandList, mCubemapPipeline);
	mDevice->BindIndexBuffer(commandList, ib.buffer);
	mDevice->DrawIndexed(commandList, meshRenderer->GetIndexCount(), 1, ib.byteOffset / sizeof(uint32_t));
}

void Renderer::UpdateLights()
{
	// Update Light Uniform Data
	auto compMgr = mScene->GetComponentManager();
	auto lightArrComponent = compMgr->GetComponentArray<LightComponent>();
	std::vector<LightComponent>& lights = lightArrComponent->components;
	std::vector<ecs::Entity>& entities = lightArrComponent->entities;
	Camera* camera = mScene->GetCamera();

	std::vector<SortedLight> sortedLights;
	std::vector<LightData> lightData;

	const float nearPlane = camera->GetNearPlane();
	const float farPlane = camera->GetFarPlane();
	const float invPlaneDistance = 1.0f / (farPlane - nearPlane);

	// Compute the distance of the light min, max and center position in 
	// the cameraSpace and convert it between 0 and 1
	for (uint32_t i = 0; i < lights.size(); ++i) {
		LightComponent& light = lights[i];
		if (!light.enabled) continue;

		// Update the lightData that is sent to buffer
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(entities[i]);
		glm::vec3 position = glm::vec3(0.0f);
		if (lights[i].type == LightType::Directional)
			position = normalize(transform->GetRotationMatrix() * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
		else if (lights[i].type == LightType::Point)
			position = transform->position;
		else assert(!"Undefined Light Type");

		lightData.emplace_back(LightData{ position, lights[i].radius, lights[i].color * lights[i].intensity, (float)lights[i].type });

		if (light.type == LightType::Directional) continue;
		float radius = light.radius;

		glm::vec3 projectedPosition = camera->ComputeViewSpaceCoordinate(position);
		glm::vec3 projectedMinPosition = projectedPosition + glm::vec3(0.0f, 0.0f, -radius);
		glm::vec3 projectedMaxPosition = projectedPosition + glm::vec3(0.0f, 0.0f,  radius);

		float z = (projectedPosition.z - nearPlane) * invPlaneDistance;
		float zMin = (projectedMinPosition.z - nearPlane) * invPlaneDistance;
		float zMax = (projectedMaxPosition.z - nearPlane) * invPlaneDistance;

		SortedLight sortedLight = {
			i, z, zMin, zMax
		};
		sortedLights.emplace_back(SortedLight{ (uint32_t)i, z, zMin, zMax });
	}

	// Sort the light by projectedZ value
	std::qsort(sortedLights.data(), sortedLights.size(), sizeof(SortedLight), [](const void* a, const void* b) {
		const SortedLight* la = (const SortedLight*)a;
		const SortedLight* lb = (const SortedLight*)b;

		if (la->projectedZ < lb->projectedZ) return -1;
		else if (la->projectedZ > lb->projectedZ) return 1;
		return 0;
		});

	// Upload LightData to GPU
	uint32_t nLights = static_cast<uint32_t>(lightData.size());
	assert(nLights <= kMaxLights);
	if(nLights > 0)
		mDevice->CopyToBuffer(mLightBuffer, lightData.data(), 0, sizeof(LightData) * nLights);
	mEnvironmentData.nLight = nLights;



	// Calculate light LUT, we use uniform distribution to sort the light into bins
	const float binSize = 1.0f / kMaxLightBins;
	for (uint16_t bin = 0; bin < kMaxLightBins; ++bin) {
		uint16_t minLightId = kMaxLights + 1;
		uint16_t maxLightId = 0;

		const float binMin = binSize * bin;
		const float binMax = binMin + binSize;

		for (uint32_t i = 0; i < sortedLights.size(); ++i) {
			SortedLight& light = sortedLights[i];

			if ((light.projectedZ >= binMin && light.projectedZ <= binMax) ||
				(light.projectedZMin >= binMin && light.projectedZMin <= binMax) || 
				(light.projectedZMax >= binMin && light.projectedZMax <= binMax)) {
				if (i < minLightId)
					minLightId = i;

				if (i > maxLightId)
					maxLightId = i;
			}
		}
		mLightLUT[bin] = (maxLightId << 16) | minLightId;
	}

	projectedLightRects.clear();
	for (uint32_t i = 0; i < lights.size(); ++i) {
		// Update the lightData that is sent to buffer
		LightComponent& light = lights[i];
		TransformComponent* transform = compMgr->GetComponent<TransformComponent>(entities[i]);
		glm::vec3 position = transform->position;
		float radius = light.radius;

		glm::vec3 vsPos = camera->ComputeViewSpaceCoordinate(position);

		bool isVisible = (vsPos.z - radius) < camera->GetNearPlane();

		if (!isVisible) continue;

		glm::vec3 aabbMin = glm::vec3(FLT_MAX);
		glm::vec3 aabbMax = glm::vec3(-FLT_MAX);
		for (uint32_t c = 0; c < 8; ++c) {
			glm::vec3 corner{
				(c % 2) ? 1.0f : -1.0f,
				(c & 2) ? 1.0f : -1.0f,
				(c & 4) ? 1.0f : -1.0f
			};
			corner *= radius;
			corner += position;

			glm::vec4 vsPos = mGlobalUniformData.V * glm::vec4(corner, 1.0f);
			vsPos.z = -std::max(camera->GetNearPlane(), -vsPos.z);

			glm::vec4 clipPos = mGlobalUniformData.P * vsPos;
			clipPos.x /= clipPos.w;
			clipPos.y /= clipPos.w;
			clipPos.z /= clipPos.w;

			aabbMin.x = glm::min(aabbMin.x, clipPos.x);
			aabbMin.y = glm::min(aabbMin.y, clipPos.y);

			aabbMax.x = glm::max(aabbMax.x, clipPos.x);
			aabbMax.y = glm::max(aabbMax.y, clipPos.y);
		}

		glm::vec4 aabb = {
			aabbMin.x, -aabbMax.y, aabbMax.x, -aabbMin.y
		};

		glm::vec4 aabbScreen{
			(aabb.x * 0.5 + 0.5) * (mSwapchainWidth - 1),
			(aabb.y * 0.5 + 0.5) * (mSwapchainHeight - 1),
			(aabb.z * 0.5 + 0.5) * (mSwapchainWidth - 1),
			(aabb.w * 0.5 + 0.5) * (mSwapchainHeight - 1),
		};

		float width = aabbScreen.z - aabbScreen.x;
		float height = aabbScreen.w - aabbScreen.y;

		if (width < 0.0001f || height < 0.0001f) continue;

		float minX = aabbScreen.x;
		float minY = aabbScreen.y;
		float maxX = minX + width;
		float maxY = minY + height;

		if (minX > mSwapchainWidth || minY > mSwapchainHeight) continue;
		if (maxX < 0.0f || maxY < 0.0f) continue;
		projectedLightRects.emplace_back(glm::vec4{ minX, minY, maxX, maxY });
	}
}
/*
void Renderer::DrawBatch(gfx::CommandList* commandList, RenderBatch& batch, uint32_t lastOffset, gfx::PipelineHandle pipeline, bool shadowPass)
{
	gfx::BufferView& vbView = batch.vertexBuffer;
	gfx::BufferView& ibView = batch.indexBuffer;

	auto vb = vbView.buffer;
	auto ib = ibView.buffer;


	// TODO: Define static Descriptor beforehand
	gfx::DescriptorInfo descriptorInfos[10] = {};

	descriptorInfos[1] = { vb, 0, mDevice->GetBufferSize(vb), gfx::DescriptorType::StorageBuffer};

	descriptorInfos[2] = { mTransformBuffer, (uint32_t)(lastOffset * sizeof(glm::mat4)), (uint32_t)(batch.transforms.size() * sizeof(glm::mat4)), gfx::DescriptorType::StorageBuffer};

	uint32_t descriptorCount = 3;
	if (shadowPass)
	{
		auto cascadeInfoBuffer = mShadowMap->mBuffer;
		descriptorInfos[0] = { cascadeInfoBuffer, 0, mDevice->GetBufferSize(cascadeInfoBuffer),gfx::DescriptorType::UniformBuffer };
	}
	else
	{
		descriptorInfos[0] = { mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
		auto& envMap = mScene->GetEnvironmentMap();
		descriptorInfos[3] = { mMaterialBuffer, (uint32_t)(lastOffset * sizeof(MaterialComponent)), (uint32_t)(batch.materials.size() * (uint32_t)sizeof(MaterialComponent)), gfx::DescriptorType::StorageBuffer };

		gfx::TextureHandle irradianceMap = envMap->GetIrradianceMap();
		descriptorInfos[4] = { &irradianceMap, 0, 0, gfx::DescriptorType::Image };

		gfx::TextureHandle prefilterMap = envMap->GetPrefilterMap();
		descriptorInfos[5] = { &prefilterMap, 0, 0, gfx::DescriptorType::Image };

		gfx::TextureHandle brdfLut = envMap->GetBRDFLUT();
		descriptorInfos[6] = { &brdfLut, 0, 0, gfx::DescriptorType::Image };

		descriptorInfos[7] = mShadowMap->GetCascadeBufferDescriptor();
		descriptorInfos[8] = mShadowMap->GetShadowMapDescriptor();

		descriptorCount += 6;

	}

	mDevice->UpdateDescriptor(pipeline, descriptorInfos, descriptorCount);
	mDevice->BindPipeline(commandList, pipeline);

	mDevice->BindIndexBuffer(commandList, ib);
	mDevice->DrawIndexedIndirect(commandList, mDrawIndirectBuffer, lastOffset * sizeof(gfx::DrawIndirectCommand), (uint32_t)batch.drawCommands.size(), sizeof(gfx::DrawIndirectCommand));
}

// Offset parameter is used to reuse the Material Buffer
void Renderer::DrawSkinnedMesh(gfx::CommandList* commandList, uint32_t offset, gfx::PipelineHandle pipeline, bool shadowPass)
{
	std::vector<DrawData> drawDatas;
	mScene->GenerateSkinnedMeshDrawData(drawDatas);
	if (drawDatas.size() == 0) return;

	auto compMgr = mScene->GetComponentManager();

	gfx::DescriptorInfo descriptorInfos[10] = {};
	uint32_t descriptorCount = 3;
	if (!shadowPass)
	{
		// Copy Materials
		std::array<gfx::TextureHandle, 64> textures;
		std::fill(textures.begin(), textures.end(), TextureCache::GetDefaultTexture());

		std::vector<MaterialComponent> materials;
		uint32_t textureIndex = 0;

		for (auto& drawData : drawDatas)
		{
			MaterialComponent material = *drawData.material;
			materials.push_back(std::move(material));
		}

		uint32_t materialCount = (uint32_t)materials.size();
		mDevice->CopyToBuffer(mMaterialBuffer, materials.data(), offset * sizeof(MaterialComponent), materialCount * sizeof(MaterialComponent));

		auto& envMap = mScene->GetEnvironmentMap();

		descriptorInfos[0] = { mGlobalUniformBuffer, 0, sizeof(GlobalUniformData), gfx::DescriptorType::UniformBuffer };
		gfx::TextureHandle irradianceMap = envMap->GetIrradianceMap();
		descriptorInfos[4] = { &irradianceMap, 0, 0, gfx::DescriptorType::Image };
		gfx::TextureHandle prefilterMap = envMap->GetPrefilterMap();
		descriptorInfos[5] = { &prefilterMap, 0, 0, gfx::DescriptorType::Image };
		gfx::TextureHandle brdfLut = envMap->GetBRDFLUT();
		descriptorInfos[6] = { &brdfLut, 0, 0, gfx::DescriptorType::Image };

		descriptorInfos[7] = mShadowMap->GetCascadeBufferDescriptor();
		descriptorInfos[8] = mShadowMap->GetShadowMapDescriptor();
		descriptorCount += 6;
	}
	else {
		gfx::BufferHandle cascadeBuffer = mShadowMap->mBuffer;
		descriptorInfos[0] = { cascadeBuffer, 0, mDevice->GetBufferSize(cascadeBuffer),gfx::DescriptorType::UniformBuffer };
	}

	uint32_t skinnedBufferOffset = 0;
	uint32_t materialOffset = offset;
	for (auto& drawData : drawDatas)
	{
		gfx::BufferView& vbView = drawData.vertexBuffer;
		gfx::BufferView& ibView = drawData.indexBuffer;

		auto& vb = vbView.buffer;
		auto& ib = ibView.buffer;

		// Copy the skinned matrix to the buffer
		auto skinnedMeshComp = compMgr->GetComponent<SkinnedMeshRenderer>(drawData.entity);
		std::vector<glm::mat4> skinnedMatrix;
		skinnedMeshComp->skeleton.GetAnimatedPose().GetMatrixPallete(skinnedMatrix);
		for (uint32_t i = 0; i < skinnedMatrix.size(); ++i)
			skinnedMatrix[i] = drawData.worldTransform * skinnedMatrix[i] * skinnedMeshComp->skeleton.GetInvBindPose(i);

		assert(skinnedMatrix.size() <= MAX_BONE_COUNT);
		uint32_t dataSize = static_cast<uint32_t>(skinnedMatrix.size()) * sizeof(glm::mat4);
		mDevice->CopyToBuffer(mSkinnedMatrixBuffer, skinnedMatrix.data(), skinnedBufferOffset, dataSize);

		// TODO: Define static Descriptor beforehand
		descriptorInfos[1] = { vb, 0, mDevice->GetBufferSize(vb), gfx::DescriptorType::StorageBuffer };
		descriptorInfos[2] = { mSkinnedMatrixBuffer, skinnedBufferOffset, dataSize, gfx::DescriptorType::UniformBuffer };
		if(!shadowPass)
			descriptorInfos[3] = { mMaterialBuffer, (uint32_t)(materialOffset * sizeof(MaterialComponent)), (uint32_t)sizeof(MaterialComponent), gfx::DescriptorType::StorageBuffer };

		mDevice->UpdateDescriptor(pipeline, descriptorInfos, descriptorCount);
		mDevice->BindPipeline(commandList, pipeline);

		mDevice->BindIndexBuffer(commandList, ib);
		mDevice->DrawIndexed(commandList, drawData.indexCount, 1, drawData.indexBuffer.offset);

		skinnedBufferOffset += dataSize;
		materialOffset += 1;
	}
}
*/
uint32_t Renderer::CreateBatch(std::vector<DrawData>& drawDatas, std::vector<RenderBatch>& renderBatch, uint32_t lastOffset)
{
	// Sort the DrawData according to bufferIndex
	std::sort(drawDatas.begin(), drawDatas.end(), [](const DrawData& lhs, const DrawData& rhs) {
		return lhs.vertexBuffer.buffer.handle < rhs.vertexBuffer.buffer.handle;
		});

	std::vector<glm::mat4> transforms;
	std::vector<MaterialComponent> materials;
	std::vector<MeshDrawData> meshDrawDatas;
	// @TODO temp
	std::vector<gfx::DrawIndirectCommand> drawIndexedCommand;

	gfx::BufferHandle lastBuffer = gfx::INVALID_BUFFER;
	RenderBatch* activeBatch = nullptr;
	uint32_t currentOffset = 0;
	if (drawDatas.size() > 0)
	{
		for (auto& drawData : drawDatas)
		{
			gfx::BufferHandle buffer = drawData.vertexBuffer.buffer;
			if (buffer.handle != lastBuffer.handle || activeBatch == nullptr)
			{
				currentOffset += activeBatch == nullptr ? 0 : activeBatch->count;

				renderBatch.push_back(RenderBatch{});
				activeBatch = &renderBatch.back();
				activeBatch->id = mBatchId++;
				activeBatch->vertexBuffer = drawData.vertexBuffer;
				activeBatch->indexBuffer = drawData.indexBuffer;
				activeBatch->meshletBuffer = drawData.meshletBuffer;
				activeBatch->meshletTriangleBuffer = drawData.meshletTriangleBuffer;
				activeBatch->meshletVertexBuffer = drawData.meshletVertexBuffer;
				activeBatch->offset = lastOffset + currentOffset;
				activeBatch->count = 0;
				activeBatch->meshletCount = 0;
				lastBuffer = buffer;
			}

			// Find texture and assign new index
			MaterialComponent material = *drawData.material;
			materials.push_back(std::move(material));

			// Update transform Data
			transforms.push_back(std::move(drawData.worldTransform));

			// Create DrawCommands
			MeshDrawData meshDrawData = {};
			meshDrawData.indexOffset = drawData.indexBuffer.byteOffset / sizeof(uint32_t);
			meshDrawData.indexCount = drawData.indexCount;
			meshDrawData.vertexOffset = drawData.vertexBuffer.byteOffset / drawData.elmSize;
			meshDrawData.vertexCount = drawData.vertexBuffer.byteLength / drawData.elmSize;
			meshDrawData.meshletCount = drawData.meshletCount;
			meshDrawData.boudingSphere = drawData.boundingSphere;
			meshDrawData.meshletOffset = drawData.meshletOffset;
			meshDrawDatas.push_back(std::move(meshDrawData));

			gfx::DrawIndirectCommand indirectCommand;
			indirectCommand.firstInstance = 0;
			indirectCommand.instanceCount = 1;
			indirectCommand.indexCount = drawData.indexCount;
			indirectCommand.firstIndex = drawData.indexBuffer.byteOffset / sizeof(uint32_t);
			indirectCommand.vertexOffset = drawData.vertexBuffer.byteOffset / drawData.elmSize;
			drawIndexedCommand.push_back(std::move(indirectCommand));

			activeBatch->count++;
			activeBatch->meshletCount += drawData.meshletCount;
		}

		// Copy data to buffer
		const uint32_t mat4Size = sizeof(glm::mat4);
		const uint32_t materialSize = sizeof(MaterialComponent);
		const uint32_t dicSize = sizeof(MeshDrawData);

		uint32_t transformCount = (uint32_t)transforms.size();
		mDevice->CopyToBuffer(mTransformBuffer, transforms.data(), lastOffset * mat4Size, transformCount * mat4Size);

		uint32_t materialCount = (uint32_t)materials.size();
		mDevice->CopyToBuffer(mMaterialBuffer, materials.data(), lastOffset * materialSize, materialCount * materialSize);

		uint32_t meshDrawDataCount = (uint32_t)meshDrawDatas.size();
		mDevice->CopyToBuffer(mMeshDrawDataBuffer, meshDrawDatas.data(), lastOffset * dicSize, meshDrawDataCount * dicSize);

		uint32_t indexedDrawCommandCount = (uint32_t)drawIndexedCommand.size();
		mDevice->CopyToBuffer(mIndexedIndirectCommandBuffer, drawIndexedCommand.data(), lastOffset * sizeof(gfx::DrawIndirectCommand), indexedDrawCommandCount * sizeof(gfx::DrawIndirectCommand));
		
		currentOffset += (activeBatch ? activeBatch->count : 0);
		lastOffset += currentOffset;
	}
	return lastOffset;
}

void Renderer::onResize(uint32_t width, uint32_t height)
{
	mFrameGraph.onResize(width, height);
	mSwapchainWidth = width;
	mSwapchainHeight = height;
}

void Renderer::Shutdown()
{
	DebugDraw::Shutdown();
	mFrameGraph.Shutdown();
	mFrameGraphBuilder.Shutdown();
	mDevice->Destroy(mFullScreenPipeline);
	mDevice->Destroy(mGlobalUniformBuffer);
	mDevice->Destroy(mLightBuffer);
	mDevice->Destroy(mTransformBuffer);
	mDevice->Destroy(mDrawIndirectBuffer);
	mDevice->Destroy(mMaterialBuffer);
	mDevice->Destroy(mSkinnedMatrixBuffer);
	mDevice->Destroy(mMeshDrawDataBuffer);
	mDevice->Destroy(mDrawCommandCountBuffer);
	mDevice->Destroy(mIndexedIndirectCommandBuffer);
	mDevice->Destroy(mCascadeInfoBuffer);
}
