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

	inline PipelineHandle CreateComputePipeline(const char* filename, gfx::GraphicsDevice* device)
	{
		uint32_t codeLen = 0;
		char* code = Utils::ReadFile(filename, &codeLen);
		if (code)
		{
			gfx::ShaderDescription shaderDesc{ code, codeLen };
			gfx::PipelineDesc pipelineDesc;
			pipelineDesc.shaderCount = 1;
			pipelineDesc.shaderDesc = &shaderDesc;

			PipelineHandle handle = device->CreateComputePipeline(&pipelineDesc);
			delete[] code;
			return handle;
		}
		return PipelineHandle{ K_INVALID_RESOURCE_HANDLE };
	}
}