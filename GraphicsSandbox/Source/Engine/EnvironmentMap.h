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
	void CalculateIrradiance();

	inline std::shared_ptr<gfx::GPUTexture> GetCubemap() { return mCubemap; }
	inline std::shared_ptr<gfx::GPUTexture> GetIrradianceMap() { return mIrradiance; }

	~EnvironmentMap() = default;
private:
	std::shared_ptr<gfx::Pipeline> mHdriToCubemap;
	std::shared_ptr<gfx::Pipeline> mIrradiancePipeline;
	gfx::GraphicsDevice* mDevice;

	const uint32_t mCubemapWidth  = 512;
	const uint32_t mCubemapHeight = 512;

	const uint32_t mIrrTexWidth  = 32;
	const uint32_t mIrrTexHeight = 32;

	std::shared_ptr<gfx::GPUTexture> mCubemap;
	std::shared_ptr<gfx::GPUTexture> mIrradiance;

};