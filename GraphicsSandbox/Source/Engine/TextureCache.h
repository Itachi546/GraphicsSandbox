#pragma once

#include <stdint.h>
#include <string>

#include "Graphics.h"

namespace TextureCache
{
	void Initialize();

	uint32_t LoadTexture(const std::string& filename);

	gfx::DescriptorInfo GetDescriptorInfo();

	void PrepareTexture(gfx::CommandList* commandList);

	void Free();
}
