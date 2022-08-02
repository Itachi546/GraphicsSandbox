#include "Components.h"
#include "GraphicsDevice.h"

void MeshDataComponent::CreateRenderData()
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	if (vertices.size() > 0 && indices.size() > 0)
	{
		// Create Staging Buffer
		gfx::GPUBufferDesc bufferDesc;
		bufferDesc.bindFlag = gfx::BindFlag::None;
		bufferDesc.usage = gfx::Usage::Upload;

		uint32_t vertexDataSize = static_cast<uint32_t>(vertices.size()) * sizeof(Vertex);
		uint32_t indexDataSize = static_cast<uint32_t>(indices.size()) * sizeof(uint32_t);
		bufferDesc.size = std::max(vertexDataSize, indexDataSize);
		
		gfx::GPUBuffer stagingBuffer;
		device->CreateBuffer(&bufferDesc, &stagingBuffer);

		// Create VertexBuffer
		bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
		bufferDesc.usage = gfx::Usage::Default;
		bufferDesc.size = vertexDataSize;

		std::memcpy(stagingBuffer.mappedDataPtr, vertices.data(), vertexDataSize);
		device->CreateBuffer(&bufferDesc, &vertexBuffer);
		device->CopyBuffer(&vertexBuffer, &stagingBuffer);

		// Create IndexBuffer
		bufferDesc.size = indexDataSize;
		bufferDesc.bindFlag = gfx::BindFlag::IndexBuffer;
		device->CreateBuffer(&bufferDesc, &indexBuffer);

		std::memcpy(stagingBuffer.mappedDataPtr, indices.data(), indexDataSize);
		device->CopyBuffer(&indexBuffer, &stagingBuffer);

	}

}
