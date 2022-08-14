#include "ImageLoader.h"
#include "Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

float* Utils::ImageLoader::LoadHDRI(const char* filename, int& width, int& height, int& nComponent)
{
	stbi_set_flip_vertically_on_load(true);
	float* data = stbi_loadf(filename, &width, &height, &nComponent, STBI_rgb_alpha);
	if (data == nullptr)
	{
		Logger::Error("Failed to load HDRI: " + std::string(filename));
		return nullptr;
	}
	return data;
}

void Utils::ImageLoader::Free(void* data)
{
	if(data)
		stbi_image_free(data);
}
