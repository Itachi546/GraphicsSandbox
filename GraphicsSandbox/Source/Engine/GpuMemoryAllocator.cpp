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

	std::shared_ptr<GPUBuffer> GpuMemoryAllocator::AllocateBuffer(GPUBufferDesc* desc, uint32_t* bufferIndex)
	{
		auto device = GetDevice();
		std::shared_ptr<GPUBuffer> buffer = std::make_shared<GPUBuffer>();
		device->CreateBuffer(desc, buffer.get());

		if(bufferIndex)
			*bufferIndex = (uint32_t)mBuffers.size();

		mBuffers.push_back(buffer);
		return mBuffers.back();
	}
	/*
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
	*/

	void GpuMemoryAllocator::CopyToBuffer(std::shared_ptr<GPUBuffer> buffer, void* data, uint32_t offset, uint32_t size, BufferView* bufferView)
	{
		if (data == nullptr)
		{
			Logger::Error("Null data to copy to buffer");
			return;
		}

		if (offset > buffer->desc.size)
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

		void* ptr = buffer->mappedDataPtr;
		if (buffer->mappedDataPtr)
		{
			uint8_t* dstLoc = reinterpret_cast<uint8_t*>(ptr) + offset;
			std::memcpy(dstLoc, data, size);
		}
		else
		{
			mStagingBufferDesc.size = size;
			GPUBuffer stagingBuffer;
			device->CreateBuffer(&mStagingBufferDesc, &stagingBuffer);
			std::memcpy(stagingBuffer.mappedDataPtr, data, size);
			device->CopyBuffer(buffer.get(), &stagingBuffer, offset);
		}
		if (bufferView)
		{
			bufferView->buffer = buffer;
			bufferView->offset = offset;
			bufferView->size = size;
		}
	}
};