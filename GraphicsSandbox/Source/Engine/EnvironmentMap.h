#pragma once

#include <memory>
#include "GraphicsDevice.h"
#include "Graphics.h"

class EnvironmentMap
{
public:
	EnvironmentMap();
	EnvironmentMap(const EnvironmentMap&) = delete;
	void operator=(const EnvironmentMap&) = delete;

	void CreateFromHDRI(const char* hdri);

	inline std::shared_ptr<gfx::GPUTexture> GetCubemap() { return mCubemap; }

	~EnvironmentMap() = default;
private:
	std::shared_ptr<gfx::Pipeline> mHdriToCubemap;
	gfx::GraphicsDevice* mDevice;

	const uint32_t mCubemapWidth  = 512;
	const uint32_t mCubemapHeight = 512;

	std::shared_ptr<gfx::GPUTexture> mCubemap;
};