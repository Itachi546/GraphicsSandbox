#include "GpuMemoryAllocator.h"
#include "GraphicsDevice.h"
#include "Logger.h"

#include <assert.h>

namespace gfx
{
	GpuMemoryAllocator::GpuMemoryAllocator()
	{
		mStagingBufferDesc.bindFlag = gfx::BindFlag::None;
		mStagingBufferDesc.usage = gfx::Usage::Upload;
	}

	GPUBuffer* GpuMemoryAllocator::AllocateBuffer(GPUBufferDesc* desc, uint32_t* bufferIndex)
	{
		auto device = GetDevice();
		std::shared_ptr<GPUBuffer> buffer = std::make_shared<GPUBuffer>();
		device->CreateBuffer(desc, buffer.get());

		*bufferIndex = (uint32_t)mBuffers.size();
		mBuffers.push_back(buffer);
		return mBuffers.back().get();
	}

	void GpuMemoryAllocator::CopyToBuffer(GPUBuffer* buffer, void* data, uint32_t offset, uint32_t size)
	{
		if (data == nullptr)
		{
			Logger::Error("Null data to copy to buffer");
			return;
		}

		if (offset > size)
		{
			Logger::Error("Invalid offset and size (offset > size)");
			return;
		}

		if (size > buffer->desc.size)
		{
			Logger::Error("Copysize exceed the size of buffer");
			return;
		}

		auto device = GetDevice();
		char* ptr = reinterpret_cast<char*>(buffer->mappedDataPtr);
		if (ptr)
			std::memcpy(ptr + offset, data, size);
		else
		{
			mStagingBufferDesc.size = size;
			GPUBuffer stagingBuffer;
			device->CreateBuffer(&mStagingBufferDesc, &stagingBuffer);
			std::memcpy(stagingBuffer.mappedDataPtr, data, size);
			device->CopyBuffer(buffer, &stagingBuffer, offset);
		}
	}
	/*
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
	*/
};