#pragma once

namespace Utils::ImageLoader
{

	float* LoadHDRI(const char* filename, int& width, int& height, int& nComponent);

	void Free(void* data);
};
