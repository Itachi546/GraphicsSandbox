#include "GraphicsUtils.h"

namespace gfx {

    PipelineHandle gfx::CreateComputePipeline(const std::string& filename, gfx::GraphicsDevice* device)
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