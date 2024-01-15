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

	inline gfx::TextureHandle GetCubemap() { return mCubemapTexture; }
	inline gfx::TextureHandle GetIrradianceMap() { return mIrradianceTexture; }
	inline gfx::TextureHandle GetPrefilterMap() { return mPrefilterTexture; }
	inline gfx::TextureHandle GetBRDFLUT() { return mBRDFTexture; }

	void Shutdown();
	virtual ~EnvironmentMap() = default;

	bool enableGroundProjection = true;
	float radius = 100.0f;
	float height = 10.0f;
private:
	gfx::PipelineHandle mHdriToCubemap;
	gfx::PipelineHandle mIrradiancePipeline;
	gfx::PipelineHandle mPrefilterPipeline;
	gfx::PipelineHandle mBRDFPipeline;
	gfx::GraphicsDevice* mDevice;

	const uint32_t mCubemapDims   = 512;
	const uint32_t mIrrTexDims    = 32;
	const uint32_t mPrefilterDims = 512;
	const uint32_t nMaxMipLevel   = 6;
	const uint32_t mBRDFDims      = 512;

	gfx::TextureHandle mCubemapTexture = gfx::INVALID_TEXTURE;
	gfx::TextureHandle mIrradianceTexture = gfx::INVALID_TEXTURE;
	gfx::TextureHandle mPrefilterTexture = gfx::INVALID_TEXTURE;
	gfx::TextureHandle mBRDFTexture = gfx::INVALID_TEXTURE;
};