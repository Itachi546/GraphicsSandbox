#pragma once

#include "Graphics.h"

#include <memory>
#include <vector>

namespace gfx
{
	/*
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

		std::shared_ptr<GPUBuffer> AllocateBuffer(GPUBufferDesc* desc, uint32_t* bufferIndex);

		void CopyToBuffer(std::shared_ptr<GPUBuffer> buffer, void* data, uint32_t offset, uint32_t size, BufferView* bufferView);
		//void CopyToBuffer(GPUBuffer* buffer, void* data, uint32_t offset, uint32_t size);

		GPUBuffer* GetBuffer(uint32_t index) const
		{
			return mBuffers[index].get();
		}

		void FreeMemory()
		{
			mBuffers.clear();
		}
	private:
		GpuMemoryAllocator();
		std::vector<std::shared_ptr<GPUBuffer>> mBuffers;
		GPUBufferDesc mStagingBufferDesc;
	};

*/
}
