#pragma once

#include "Graphics.h"
#include "GraphicsDevice.h"

#include "Utils.h"

namespace gfx
{
	inline uint32_t GetWorkSize(uint32_t width, uint32_t localWorkSize)
	{
		return (width + localWorkSize - 1) / localWorkSize;
	}

	PipelineHandle CreateComputePipeline(const std::string& filename, gfx::GraphicsDevice* device);
	PipelineHandle CreateGraphicsPipeline();
}