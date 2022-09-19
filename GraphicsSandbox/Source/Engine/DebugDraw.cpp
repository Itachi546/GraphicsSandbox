#include "DebugDraw.h"
#include "GraphicsDevice.h"
#include "Graphics.h"
#include "Utils.h"

bool gEnableDebugDraw = true;
gfx::Pipeline* gPipeline;
gfx::GPUBuffer* gBuffer;
uint32_t gDataOffset = 0;
const uint32_t kMaxDebugData = 10'000;
struct DebugData
{
	glm::vec3 position;
	uint32_t color;
};
DebugData* dataPtr = nullptr;

void DebugDraw::Initialize(gfx::RenderPass* renderPass)
{
	gfx::PipelineDesc pipelineDesc;

	uint32_t fileSize = 0;
	gfx::ShaderDescription shaderDesc[2] = {};
	shaderDesc[0].code = Utils::ReadFile("Assets/SPIRV/debug.vert.spv", &fileSize);
	shaderDesc[0].sizeInByte = fileSize;
	shaderDesc[1].code = Utils::ReadFile("Assets/SPIRV/debug.frag.spv", &fileSize);
	shaderDesc[1].sizeInByte = fileSize;
	pipelineDesc.shaderCount = (uint32_t)std::size(shaderDesc);
	pipelineDesc.shaderDesc = shaderDesc;
	pipelineDesc.rasterizationState.cullMode = gfx::CullMode::None;
	pipelineDesc.rasterizationState.enableDepthTest = true;
	pipelineDesc.rasterizationState.enableDepthWrite = true;
	pipelineDesc.topology = gfx::Topology::Line;
	pipelineDesc.renderPass = renderPass;
	gfx::BlendState blendState = {};
	pipelineDesc.blendState = &blendState;
	pipelineDesc.blendStateCount = 1;
	pipelineDesc.rasterizationState.lineWidth = 2.0f;
	
	gfx::GraphicsDevice* device = gfx::GetDevice();
	gPipeline = new gfx::Pipeline;
	device->CreateGraphicsPipeline(&pipelineDesc, gPipeline);


	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = kMaxDebugData * sizeof(DebugData);
	gBuffer = new gfx::GPUBuffer;
	device->CreateBuffer(&bufferDesc, gBuffer);
	dataPtr = reinterpret_cast<DebugData*>(gBuffer->mappedDataPtr);
}

void DebugDraw::SetEnable(bool state)
{
	gEnableDebugDraw = state;
}

void DebugDraw::AddLine(const glm::vec3& start, const glm::vec3& end, uint32_t color)
{
	if (!gEnableDebugDraw)
		return;

	assert(dataPtr != nullptr);
	DebugData* data = dataPtr + gDataOffset;
	data->position = start;
	data->color = color;
	gDataOffset++;

	data = data + 1;
	data->position = end;
	data->color = color;
	gDataOffset++;
}

void DebugDraw::Draw(gfx::CommandList* commandList, glm::mat4 VP)
{
	if (gEnableDebugDraw && gDataOffset > 0)
	{
		auto device = gfx::GetDevice();
		gfx::DescriptorInfo descriptorInfo = {gBuffer, 0, gDataOffset * sizeof(DebugData), gfx::DescriptorType::StorageBuffer};
		device->UpdateDescriptor(gPipeline, &descriptorInfo, 1);
		device->BindPipeline(commandList, gPipeline);
		device->PushConstants(commandList, gPipeline, gfx::ShaderStage::Vertex, &VP[0][0], sizeof(glm::mat4), 0);
		device->Draw(commandList, gDataOffset, 0, 1);
	}
	gDataOffset = 0;
}

void DebugDraw::Free()
{
	delete gPipeline;
	delete gBuffer;
}