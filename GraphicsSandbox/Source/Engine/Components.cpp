#include "Components.h"
#include "GraphicsDevice.h"

void MeshRenderer::CopyToGPU(gfx::GpuMemoryAllocator* allocator, std::shared_ptr<gfx::GPUBuffer> vertexBuffer, std::shared_ptr<gfx::GPUBuffer> indexBuffer, uint32_t vbOffset, uint32_t ibOffset)
{
	allocator->CopyToBuffer(&this->vertexBuffer, vertexBuffer, vertices->data(), vbOffset, (uint32_t)vertices->size() * sizeof(Vertex));
	allocator->CopyToBuffer(&this->indexBuffer, indexBuffer, indices->data(),  ibOffset, (uint32_t)indices->size() * sizeof(uint32_t));
}
