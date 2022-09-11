#pragma once

#include <stdint.h>
#include <string>

#include "Graphics.h"

namespace TextureCache
{
	void Initialize();

	uint32_t LoadTexture(const std::string& filename);

	gfx::GPUTexture* GetDefaultTexture();

	gfx::GPUTexture* GetByIndex(uint32_t index);

	gfx::DescriptorInfo GetDescriptorInfo();

	void PrepareTexture(gfx::CommandList* commandList);

	void Free();
}
