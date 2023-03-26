#include "DebugDraw.h"
#include "GraphicsDevice.h"
#include "Graphics.h"
#include "Utils.h"

bool gEnableDebugDraw = true;
gfx::PipelineHandle gPipeline;
gfx::BufferHandle gBuffer;
uint32_t gDataOffset = 0;
const uint32_t kMaxDebugData = 10'000;
struct DebugData
{
	glm::vec3 position;
	uint32_t color;
};
DebugData* dataPtr = nullptr;

void DebugDraw::Initialize(gfx::RenderPassHandle renderPass)
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
	pipelineDesc.rasterizationState.lineWidth = 1.0f;
	
	gfx::GraphicsDevice* device = gfx::GetDevice();
	gPipeline = device->CreateGraphicsPipeline(&pipelineDesc); 

	delete[] shaderDesc[0].code;
	delete[] shaderDesc[1].code;

	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = kMaxDebugData * sizeof(DebugData);
	gBuffer = device->CreateBuffer(&bufferDesc);
	dataPtr = reinterpret_cast<DebugData*>(device->GetMappedDataPtr(gBuffer));
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

void DebugDraw::AddAABB(const glm::vec3& min, const glm::vec3& max, uint32_t color)
{
	if (!gEnableDebugDraw)
		return;

	// Bottom
	AddLine(min, glm::vec3(max.x, min.y, min.z), color);
	AddLine(min, glm::vec3(min.x, min.y, max.z), color);
	AddLine(glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z), color);
	AddLine(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, min.y, min.z), color);

	//Top
	AddLine(glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z), color);
	AddLine(glm::vec3(min.x, max.y, min.z), glm::vec3(min.x, max.y, max.z), color);
	AddLine(glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z), color);
	AddLine(glm::vec3(max.x, max.y, max.z), glm::vec3(max.x, max.y, min.z), color);

	// Joint
	AddLine(min, glm::vec3(min.x, max.y, min.z), color);
	AddLine(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z), color);
	AddLine(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z), color);
	AddLine(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z), color);
}

void DebugDraw::AddFrustum(const glm::vec3* points, uint32_t count, uint32_t color)
{
	if (!gEnableDebugDraw)
		return;

	// Near Plane
	AddLine(points[0], points[1], color);
	AddLine(points[0], points[3], color);
	AddLine(points[1], points[2], color);
	AddLine(points[3], points[2], color);

	// Far Plane
	AddLine(points[4], points[5], color);
	AddLine(points[4], points[7], color);
	AddLine(points[5], points[6], color);
	AddLine(points[7], points[6], color);

	// Joint
	AddLine(points[0], points[4], color);
	AddLine(points[1], points[5], color);
	AddLine(points[3], points[7], color);
	AddLine(points[2], points[6], color);
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
}
