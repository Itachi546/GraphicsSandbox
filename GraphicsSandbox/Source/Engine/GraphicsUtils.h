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

	inline void CreateComputePipeline(const char* filename, gfx::GraphicsDevice* device, gfx::Pipeline* out)
	{
		uint32_t codeLen = 0;
		char* code = Utils::ReadFile(filename, &codeLen);
		gfx::ShaderDescription shaderDesc{ code, codeLen };
		gfx::PipelineDesc pipelineDesc;
		pipelineDesc.shaderCount = 1;
		pipelineDesc.shaderDesc = &shaderDesc;

		device->CreateComputePipeline(&pipelineDesc, out);
		delete[] code;
	}

}