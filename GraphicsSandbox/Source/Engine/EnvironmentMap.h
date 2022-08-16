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
	// Create Irradiance Texture from EnvironmentMap
	void CalculateIrradiance();
	// Generate prefiltered environment map for varying roughness
	void Prefilter();
	// Generate BRDF LUT
	void CalculateBRDFLUT();

	inline std::shared_ptr<gfx::GPUTexture> GetCubemap() { return mCubemapTexture; }
	inline std::shared_ptr<gfx::GPUTexture> GetIrradianceMap() { return mIrradianceTexture; }
	inline std::shared_ptr<gfx::GPUTexture> GetPrefilterMap() { return mPrefilterTexture; }
	inline std::shared_ptr<gfx::GPUTexture> GetBRDFLUT() { return mBRDFTexture; }

	~EnvironmentMap() = default;
private:
	std::shared_ptr<gfx::Pipeline> mHdriToCubemap;
	std::shared_ptr<gfx::Pipeline> mIrradiancePipeline;
	std::shared_ptr<gfx::Pipeline> mPrefilterPipeline;
	std::shared_ptr<gfx::Pipeline> mBRDFPipeline;
	gfx::GraphicsDevice* mDevice;

	const uint32_t mCubemapDims   = 512;
	const uint32_t mIrrTexDims    = 32;
	const uint32_t mPrefilterDims = 512;
	const uint32_t nMaxMipLevel   = 6;
	const uint32_t mBRDFDims = 512;

	std::shared_ptr<gfx::GPUTexture> mCubemapTexture;
	std::shared_ptr<gfx::GPUTexture> mIrradianceTexture;
	std::shared_ptr<gfx::GPUTexture> mPrefilterTexture;
	std::shared_ptr<gfx::GPUTexture> mBRDFTexture;
};