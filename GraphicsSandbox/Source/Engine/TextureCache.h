#pragma once

#include <stdint.h>
#include <string>

#include "Graphics.h"

namespace TextureCache
{
	void Initialize();

	uint32_t LoadTexture(const std::string& filename, bool generateMipmap = false);

	std::string GetTextureName(uint32_t index);

	gfx::TextureHandle GetDefaultTexture();

	gfx::TextureHandle GetByIndex(uint32_t index);

	gfx::DescriptorInfo GetDescriptorInfo();

	void Shutdown();
}
