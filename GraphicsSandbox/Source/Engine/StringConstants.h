#pragma once

namespace StringConstants
{
	// GBuffer Pass
	constexpr const char* GBUFFER_FRAG_PATH = "Assets/SPIRV/gbuffer.frag.spv";

	// Light Pass
	constexpr const char* LIGHTING_FRAG_PATH = "Assets/SPIRV/lighting.frag.spv";

	// Shaders
	constexpr const char* TRANSPARENT_VERT_PATH = "Assets/SPIRV/transparent.vert.spv";
	constexpr const char* MAIN_VERT_PATH = "Assets/SPIRV/main.vert.spv";
	constexpr const char* MAIN_FRAG_PATH = "Assets/SPIRV/main.frag.spv";

	constexpr const char* CUBEMAP_VERT_PATH = "Assets/SPIRV/cubemap.vert.spv";
	constexpr const char* CUBEMAP_FRAG_PATH = "Assets/SPIRV/cubemap.frag.spv";

	constexpr const char* FULLSCREEN_VERT_PATH = "Assets/SPIRV/fullscreen.vert.spv";
	constexpr const char* FULLSCREEN_FRAG_PATH = "Assets/SPIRV/fullscreen.frag.spv";

	// FXAA
	constexpr const char* FXAA_FRAG_PATH = "Assets/SPIRV/fxaa.frag.spv";

	// EnvironmentMap
	constexpr const char* HDRI_CONVERTER_COMP_PATH = "Assets/SPIRV/hdri_converter.comp.spv";
	constexpr const char* IRRADIANCE_COMP_PATH = "Assets/SPIRV/irradiance.comp.spv";
	constexpr const char* PREFILTER_COMP_PATH = "Assets/SPIRV/prefilter_env.comp.spv";
	constexpr const char* BRDF_LUT_COMP_PATH = "Assets/SPIRV/brdf_lut.comp.spv";

	// HDRI
	constexpr const char* HDRI_PATH = "Assets/EnvironmentMap/sunset.hdr";

	// Bloom
	constexpr const char* BLOOM_UPSAMPLE_PATH = "Assets/SPIRV/bloom-upsample.comp.spv";
	constexpr const char* BLOOM_DOWNSAMPLE_PATH = "Assets/SPIRV/bloom-downsample.comp.spv";
	constexpr const char* BLOOM_COMPOSITE_PATH = "Assets/SPIRV/bloom-composite.comp.spv";
}