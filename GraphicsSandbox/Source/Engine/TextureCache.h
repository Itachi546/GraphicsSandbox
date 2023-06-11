#pragma once

#include <stdint.h>
#include <string>

#include "Graphics.h"

namespace TextureCache
{
	void Initialize();

	uint32_t LoadTexture(const std::string& filename, bool generateMipmap = false);

	uint32_t LoadTexture(const std::string& name, uint32_t width, uint32_t height, unsigned char* data, uint32_t nChannel, bool generateMipmap = false);

	std::string GetTextureName(uint32_t index);

	gfx::TextureHandle GetDefaultTexture();

	gfx::TextureHandle GetByIndex(uint32_t index);

	gfx::DescriptorInfo GetDescriptorInfo();

	void Shutdown();
}
