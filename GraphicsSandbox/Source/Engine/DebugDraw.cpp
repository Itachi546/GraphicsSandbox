#include "DebugDraw.h"
#include "GraphicsDevice.h"
#include "Graphics.h"
#include "Utils.h"

#include <algorithm>

bool gEnableDebugDraw = false;
gfx::PipelineHandle gPipeline = gfx::INVALID_PIPELINE;
gfx::BufferHandle gBuffer = gfx::INVALID_BUFFER;

gfx::BufferHandle gPrimitiveBuffer = gfx::INVALID_BUFFER;
gfx::PipelineHandle gPrimitivePipeline = gfx::INVALID_PIPELINE;

uint32_t gDataOffset = 0;
uint32_t gPrimBufferOffset = 0;

const uint32_t kMaxDebugData = 100'000;

struct DebugData
{
	glm::vec3 position;
	uint32_t color;
};

struct Quad {
	glm::vec3 p0;
	glm::vec3 p1;
	glm::vec3 p2;
	glm::vec3 p3;
	uint32_t color;

	glm::vec3 getCenter() const {
		return p0 + (p2 - p0) * 0.5f;
	}
};

std::vector<Quad> gQuadPrimitive;

DebugData* gDataBufferPtr = nullptr;
DebugData* gPrimitiveBufferPtr = nullptr;

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
	blendState.enable = true;
	pipelineDesc.blendStates = &blendState;
	pipelineDesc.blendStateCount = 1;
	pipelineDesc.rasterizationState.lineWidth = 2.0f;
	
	gfx::GraphicsDevice* device = gfx::GetDevice();
	gPipeline = device->CreateGraphicsPipeline(&pipelineDesc); 

	pipelineDesc.topology = gfx::Topology::TriangleList;
	gPrimitivePipeline = device->CreateGraphicsPipeline(&pipelineDesc);


	delete[] shaderDesc[0].code;
	delete[] shaderDesc[1].code;

	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = kMaxDebugData * sizeof(DebugData);
	gBuffer = device->CreateBuffer(&bufferDesc);
	gDataBufferPtr = reinterpret_cast<DebugData*>(device->GetMappedDataPtr(gBuffer));

	gPrimitiveBuffer = device->CreateBuffer(&bufferDesc);
	gPrimitiveBufferPtr = reinterpret_cast<DebugData*>(device->GetMappedDataPtr(gPrimitiveBuffer));

}

void DebugDraw::SetEnable(bool state)
{
	gEnableDebugDraw = state;
}

void DebugDraw::AddLine(const glm::vec3& start, const glm::vec3& end, uint32_t color)
{
	if (!gEnableDebugDraw)
		return;

	assert(gDataBufferPtr != nullptr);
	DebugData* data = gDataBufferPtr + gDataOffset;
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

void DebugDraw::AddSphere(const glm::vec3& p, float radius, uint32_t color)
{
	// Create a circle and rotate it 
	glm::mat3 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	uint32_t nPoint = 40;
	float dTheta = (glm::pi<float>() * 2.0f) / float(nPoint);

	float theta = 0.0f;
	glm::vec3 lastPosition = radius * glm::vec3(cos(theta), sin(theta), 0.0f);
	for (uint32_t i = 1; i <= nPoint; ++i) {
		theta += dTheta;
		glm::vec3 currentPosition = radius * glm::vec3(cos(theta), sin(theta), 0.0f);
		AddLine(lastPosition + p, currentPosition + p, color);
		AddLine(rotate * lastPosition + p, rotate * currentPosition + p, color);
		AddLine(glm::vec3(lastPosition.x + p.x, lastPosition.z + p.y, lastPosition.y + p.z),
			glm::vec3(currentPosition.x + p.x, currentPosition.z + p.y, currentPosition.y + p.z), color);

		lastPosition = currentPosition;
	}
}

void DebugDraw::AddQuadPrimitive(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, uint32_t color)
{
	if (!gEnableDebugDraw)
		return;

	gQuadPrimitive.emplace_back(Quad{ p0, p1, p2, p3, color });

}

void DebugDraw::AddFrustumPrimitive(const std::array<glm::vec3, 8>& points, uint32_t color)
{
	const glm::vec3& ntl = points[0];
	const glm::vec3& ntr = points[1];
	const glm::vec3& nbr = points[2];
	const glm::vec3& nbl = points[3];

	const glm::vec3& ftl = points[4];
	const glm::vec3& ftr = points[5];
	const glm::vec3& fbr = points[6];
	const glm::vec3& fbl = points[7];

	// Near plane
	AddQuadPrimitive(ntl, ntr, nbr, nbl, color);

	// Far plane
	AddQuadPrimitive(ftl, ftr, fbr, fbl, color);

	// Top Plane
	AddQuadPrimitive(ntl, ftl, ftr, ntr, color);

	// Bottom plane
	AddQuadPrimitive(nbl, fbl, fbr, nbr, color);

	// Left plane
	AddQuadPrimitive(nbl, ntl, ftl, fbl, color);

	// Right plane
	AddQuadPrimitive(nbr, ntr, ftr, fbr, color);
}

static void AddQuadInternal(Quad& quad) {

	assert(gPrimitiveBufferPtr != nullptr);

	DebugData* data = gPrimitiveBufferPtr + gPrimBufferOffset;

	data->position = quad.p0;
	data->color = quad.color;
	gPrimBufferOffset++;
	data++;

	data->position = quad.p1;
	data->color = quad.color;
	gPrimBufferOffset++;
	data++;

	data->position = quad.p2;
	data->color = quad.color;
	gPrimBufferOffset++;
	data++;

	data->position = quad.p0;
	data->color = quad.color;
	gPrimBufferOffset++;
	data++;

	data->position = quad.p2;
	data->color = quad.color;
	gPrimBufferOffset++;
	data++;

	data->position = quad.p3;
	data->color = quad.color;
	gPrimBufferOffset++;
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


void DebugDraw::Draw(gfx::CommandList* commandList, glm::mat4 VP, const glm::vec3& camPos)
{
	if (gQuadPrimitive.size() > 0) {

		std::sort(gQuadPrimitive.begin(), gQuadPrimitive.end(), [&camPos](const Quad& a, const Quad& b) {

			float d1 = glm::distance2(a.getCenter(), camPos);
			float d2 = glm::distance2(b.getCenter(), camPos);
			return d1 > d2;
		});

		for (auto& quad : gQuadPrimitive)
			AddQuadInternal(quad);

		gQuadPrimitive.clear();
	}
	if (gEnableDebugDraw)
	{
		auto device = gfx::GetDevice();
		if (gDataOffset > 0) {
			gfx::DescriptorInfo descriptorInfo = { gBuffer, 0, gDataOffset * sizeof(DebugData), gfx::DescriptorType::StorageBuffer };
			device->UpdateDescriptor(gPipeline, &descriptorInfo, 1);
			device->BindPipeline(commandList, gPipeline);
			device->PushConstants(commandList, gPipeline, gfx::ShaderStage::Vertex, &VP[0][0], sizeof(glm::mat4), 0);
			device->Draw(commandList, gDataOffset, 0, 1);
		}

		if (gPrimBufferOffset > 0) {
			gfx::DescriptorInfo descriptorInfo = { gPrimitiveBuffer, 0, gPrimBufferOffset * sizeof(DebugData), gfx::DescriptorType::StorageBuffer };
			device->UpdateDescriptor(gPrimitivePipeline, &descriptorInfo, 1);
			device->BindPipeline(commandList, gPrimitivePipeline);
			device->PushConstants(commandList, gPrimitivePipeline, gfx::ShaderStage::Vertex, &VP[0][0], sizeof(glm::mat4), 0);
			device->Draw(commandList, gPrimBufferOffset, 0, 1);
		}

	}
	gPrimBufferOffset = 0;
	gDataOffset = 0;
}

void DebugDraw::Shutdown()
{
	gfx::GetDevice()->Destroy(gBuffer);
	gfx::GetDevice()->Destroy(gPrimitiveBuffer);
	gfx::GetDevice()->Destroy(gPipeline);
	gfx::GetDevice()->Destroy(gPrimitivePipeline);
}
