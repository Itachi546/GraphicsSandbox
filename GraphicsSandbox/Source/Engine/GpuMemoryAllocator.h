#pragma once

#include "Graphics.h"

#include <memory>
#include <vector>

namespace gfx
{
	using BufferIndex = uint32_t;

	struct BufferView
	{
		BufferIndex index;
		uint32_t offset;
		uint32_t size;
	};

	class GpuMemoryAllocator
	{
	public:
		static GpuMemoryAllocator* GetInstance()
		{
			static GpuMemoryAllocator allocator;
			return &allocator;
		}

		GpuMemoryAllocator(const GpuMemoryAllocator&) = delete;
		void operator=(const GpuMemoryAllocator&) = delete;

		BufferIndex AllocateBuffer(GPUBufferDesc* desc);

		void CopyToBuffer(BufferView* bufferView, BufferIndex buffer, void* data, uint32_t size);

		GPUBuffer* GetBuffer(uint32_t index) const
		{
			return mBuffers[index].buffer.get();
		}

		void FreeMemory()
		{
			mBuffers.clear();
		}
	private:
		GpuMemoryAllocator();

		struct BufferInfo
		{
			std::shared_ptr<GPUBuffer> buffer;
			uint32_t offset;
		};

		std::vector<BufferInfo> mBuffers;
		GPUBufferDesc mStagingBufferDesc;
	};

}