#include "GpuMemoryAllocator.h"
#include "GraphicsDevice.h"

#include <assert.h>

namespace gfx
{
	GpuMemoryAllocator::GpuMemoryAllocator()
	{
		mStagingBufferDesc.bindFlag = gfx::BindFlag::None;
		mStagingBufferDesc.usage = gfx::Usage::Upload;
	}

	BufferIndex GpuMemoryAllocator::AllocateBuffer(GPUBufferDesc* desc)
	{
		auto device = GetDevice();
		std::shared_ptr<GPUBuffer> buffer = std::make_shared<GPUBuffer>();
		device->CreateBuffer(desc, buffer.get());

		BufferIndex bufferIndex = static_cast<uint32_t>(mBuffers.size());
		BufferInfo bufferInfo;
		bufferInfo.buffer = buffer;
		bufferInfo.offset = 0;
		mBuffers.push_back(std::move(bufferInfo));
		return bufferIndex;
	}

	void GpuMemoryAllocator::CopyToBuffer(BufferView* bufferView, BufferIndex bufferIndex, void* data, uint32_t size)
	{
		assert(data != nullptr);
		assert(bufferIndex < mBuffers.size());

		auto device = GetDevice();
		BufferInfo& bufferInfo = mBuffers[bufferIndex];
		uint32_t memorySize = bufferInfo.buffer->desc.size - bufferInfo.offset;
		assert(memorySize > 0);
		assert(memorySize >= size);

		void* ptr = bufferInfo.buffer->mappedDataPtr;
		if (bufferInfo.buffer->mappedDataPtr)
		{
			uint8_t* dstLoc = reinterpret_cast<uint8_t*>(ptr) + bufferInfo.offset;
			std::memcpy(dstLoc, data, size);
		}
		else
		{
			mStagingBufferDesc.size = size;
			GPUBuffer stagingBuffer;
			device->CreateBuffer(&mStagingBufferDesc, &stagingBuffer);
			std::memcpy(stagingBuffer.mappedDataPtr, data, size);

			device->CopyBuffer(bufferInfo.buffer.get(), &stagingBuffer, bufferInfo.offset);
		}
		
		bufferView->index = bufferIndex;
		bufferView->offset = bufferInfo.offset;
		bufferView->size = size;
		bufferInfo.offset += size;

	}

};