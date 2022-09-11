#pragma once

#include <cstdint>

namespace Utils::ImageLoader
{

	float* LoadHDRI(const char* filename, int& width, int& height, int& nComponent);

	uint8_t* Load(const char* filename, int& width, int& height, int& nComponent);


	void Free(void* data);
};
